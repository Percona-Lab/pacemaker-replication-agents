#!/bin/bash
#
# Test description
# 
# Test the async_stop feature
#
# This test assumes the master is running under LXC 
# and the root device is a ceph rbd... so be aware...
#


# global include files
. config
. functions

testdir=`dirname $0`

verbose=1

print_verbose() {
   if [ "$verbose" -eq "1" ]; then
        echo $1
   fi
}

setup() {

allsetup

} 

runtest() {
    local rootdev rootdevmajmin
    local isOnline mysqldRunning mysqldRunning2

    build_assoc_uname_ssh
    master=`check_master`
    print_verbose "Master is $master"

    # create the table with many large SKs
    (cat <<EOF
create database if not exists test;
use test;
drop table if exists test_prm_async_stop; 
create table test_prm_async_stop (
    id int NOT NULL AUTO_INCREMENT,
    data varchar(36),
    datamd5 binary(32),
    datasha1 binary(40),
    primary key(id),
    key idx_data (data),
    key idx_datamd5 (datamd5),
    key idx_datasha1 (datasha1),
    key idx_compound (data,datamd5,datasha1)
    ) engine=innodb;
set global innodb_flush_log_at_trx_commit=0;
set global innodb_flush_neighbors=0;
EOF
    ) | ${uname_ssh[$master]} "sudo $MYSQL" 2> /dev/null 

    print_verbose "Insert a set of rows"
    # insert some rows
    ( for i in `seq 1 10000`; do
         uuid=`uuidgen`
         echo "insert into test.test_prm_async_stop (data,datamd5,datasha1) values ('$uuid',md5('$uuid'),sha1('$uuid'));" 
      done
    ) | ${uname_ssh[$master]} "sudo $MYSQL"

    # Give it some time to cleanup
    sleep 5

    print_verbose "Throttling iops"
    # now, we throttle the iops
    throttle_iops $master 10

    print_verbose "doing a large update"
    # The tablespace are fully allocated, let's generate some dirty pages, this normally produces 600+ dirty pages, about 1min 
    # of io bound write load at 10 iops
    echo "update test.test_prm_async_stop set datamd5=md5(datamd5), datasha1=sha1(datasha1) where id % 5 = 0; update test.test_prm_async_stop set datamd5=md5(datamd5), datasha1=sha1(datasha1) where id % 5 = 1;update test.test_prm_async_stop set datamd5=md5(datamd5), datasha1=sha1(datasha1) where id % 5 = 2;update test.test_prm_async_stop set datamd5=md5(datamd5), datasha1=sha1(datasha1) where id % 5 = 3;update test.test_prm_async_stop set datamd5=md5(datamd5), datasha1=sha1(datasha1) where id % 5 = 4;" | ${uname_ssh[$master]} "sudo $MYSQL"

    print_verbose "Master in standby"
    # At this point, the MySQL instance has many dirty pages and many entries in the change buffer
    # Let's put the node in standby
    ${uname_ssh[$master]} "sudo crm node standby $master"

    # Give it some time
    sleep 15 

    # Confirm the node is in standby and that mysqld is still running
    declare -a online_nodes=( `crm_nodes` ) 
    isOnline=`echo ${online_nodes[@]} | grep -c $master`  # should be 0
    # Sanity check
    if [ "$isOnline" -gt 0 ]; then
	throttle_iops $master 100000
    	${uname_ssh[$master]} "sudo crm node online $master"
        local_cleanup
        print_result "$0 Master is still online" $PRM_FAIL
    fi
    
    mysqldRunning=`${uname_ssh[$master]} "pidof mysqld"` # should be defined

    # Sanity check
    if [ ! -n "$mysqldRunning" ]; then
	throttle_iops $master 100000
    	${uname_ssh[$master]} "sudo crm node online $master"
        local_cleanup
        print_result "$0 MySQL is not running" $PRM_FAIL
    fi
    print_verbose "Previous master in standby and mysqld still running ($mysqldRunning), putting it back online"

    # Let's put the node back online 
    ${uname_ssh[$master]} "sudo crm node online $master"

    # Give it some time
    sleep 5

    # Confirm the node is in online and that mysqld is still running
    declare -a online_nodes=( `crm_nodes` ) 
    isOnline=`echo ${online_nodes[@]} | grep -c $master`  # should be 1
    # Sanity check
    if [ "$isOnline" -eq 0 ]; then
	throttle_iops $master 100000
        local_cleanup
        print_result "$0 Master is still standby" $PRM_FAIL
    fi
    
    mysqldRunning2=`${uname_ssh[$master]} "pidof mysqld"` # should be defined

    # Sanity check
    if [ "$mysqldRunning" -ne "$mysqldRunning2" ]; then
	throttle_iops $master 100000
        local_cleanup
        print_result "$0 MySQL has different pid, should still be stopping" $PRM_FAIL
    fi

    print_verbose "Mysql is still stopping, no startup"
    
    # Release the iops limitations
    throttle_iops $master 100000

    # Wait a bit to complete
    sleep 30 

    #retest pid of mysql
    mysqldRunning2=`${uname_ssh[$master]} "pidof mysqld"` # should be defined
    : ${mysqldRunning2}=0

    if [ "$mysqldRunning" -eq "$mysqldRunning2" ]; then
        local_cleanup
        print_result "$0 MySQL has still the same pid" $PRM_FAIL
    else
	print_result "$0" $PRM_SUCCESS
    fi

}

local_cleanup() {

    online_all

    #now, a bit of cleanup before ending
    (cat <<EOF
create database if not exists test;
use test;
drop table if exists test_prm_kill_sessions; 
EOF
    ) | ${uname_ssh[$master]} "sudo $MYSQL"
    ${uname_ssh[$master]} "sudo crm resource cleanup p_mysql"
}

cleanup() {

allcleanup    

}


# What kind of method was invoked?
case "$1" in
  setup)    setup;;
  runtest)  runtest;;
  cleanup)  cleanup;;

 *)     echo 'Non implemented'
        exit 1
esac
