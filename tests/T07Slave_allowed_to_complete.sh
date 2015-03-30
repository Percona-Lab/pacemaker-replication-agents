#!/bin/bash
#
# Test description
# 
# Test that slaves are allowed to complete applying their relay log before 
# unset master

# global include files
. config
. functions

testdir=`dirname $0`


setup() {

./T01gracefulstart.sh setup

} 

runtest() {
    local master newmaster
    local count_result

    build_assoc_uname_ssh
    master=`check_master`

    # create the table and insert with a sleep
    (cat <<EOF
create database if not exists test;
use test;
drop table if exists test_prm_slave_complete; 
create table test_prm_slave_complete (
    id int NOT NULL AUTO_INCREMENT,
    data varchar(30),
    data1 int,
    primary key(id));
/*!50100 set global binlog_format='STATEMENT' */;
insert into test_prm_slave_complete (data,data1) select '${master}', sleep(15);
EOF
    ) | ${uname_ssh[$master]} "sudo $MYSQL"

    # Now, the slave are stuck applying the insert, put the master in standby
    ${uname_ssh[$master]} "sudo crm node standby $master"

    # wait for the sleep to complete and promotion
    sleep 20

    newmaster=`check_master`
    # Sanity check
    if [ "$master" = "$newmaster" ]; then
	local_cleanup
        print_result "$0 Still the same master" $PRM_FAIL
    fi

    count_result=`(cat <<EOF
use test;
select count(*) from test_prm_slave_complete where data = '$master';
EOF
    ) | ${uname_ssh[$newmaster]} "sudo $MYSQL -N"`

    local_cleanup
    if [ "$count_result" -ne 1 ]; then
        print_result "$0" $PRM_FAIL
    else 
        print_result "$0" $PRM_SUCCESS
    fi

}

local_cleanup() {
    #now, a bit of cleanup before ending
    newmaster=`check_master`
    (cat <<EOF
create database if not exists test;
use test;
drop table if exists test_prm_slave_complete; 
EOF
    ) | ${uname_ssh[$newmaster]} "sudo $MYSQL"

}

cleanup() {

./T01gracefulstart.sh cleanup    

}


# What kind of method was invoked?
case "$1" in
  setup)    setup;;
  runtest)  runtest;;
  cleanup)  cleanup;;

 *)     echo 'Non implemented'
        exit 1
esac
