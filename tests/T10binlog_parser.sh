#!/bin/bash
#
# Test description
# 
# Test the prm_binlog_parser feature
#
#


# global include files
. config
. functions

testdir=`dirname $0`


setup() {

allsetup

} 

runtest() {
    local IPSlave0 newmaster cnt

    build_assoc_uname_ssh
    master=`check_master`

    # create the table with many large SKs
    (cat <<EOF
create database if not exists test;
use test;
drop table if exists test_prm_binlog_parser; 
create table test_prm_binlog_parser (
    id int NOT NULL AUTO_INCREMENT,
    primary key (id)
    ) engine=innodb;
EOF
    ) | ${uname_ssh[$master]} "sudo $MYSQL" 2> /dev/null 

    # Make sure it is replicated
    sleep 1

    declare -a slaves=( `crm_slaves` )

    # get the IP of the first slave
    IPSlave0=`${uname_ssh[${slaves[0]}]} "ifconfig eth0" | grep 'inet adr' | cut -d':' -f2 | cut -d' ' -f1`

    # Block network trafic toward ${slaves[0]} coming from 3306
    ${uname_ssh[$master]} "sudo iptables -I OUTPUT -m tcp -p tcp -d $IPSlave0 --sport 3306 -j DROP"

    # now we insert one row
    echo "insert into test.test_prm_binlog_parser (id) values (1);" | ${uname_ssh[$master]} "sudo $MYSQL"

    # Give it some time to replicate to slave[1]
    sleep 1

    # We simulate a master crash
    ${uname_ssh[$master]} 'ps fax | egrep "corosync|pacemakerd|mysqld|/usr/lib/pacemaker" | grep -v egrep | awk '\''{ print $1 }'\'' | xargs sudo kill -9'

    # Give some time for Pacemaker to react
    sleep 40  # corosync token = 3000 and token_retransmits_before_loss_const = 10 so need to be bigger than 30s

    newmaster=`check_master`
    
    if [ "$master" == "$newmaster" ]; then
	local_cleanup
        print_result "$0 Still the same master" $PRM_FAIL
    fi

    if [ "${slaves[0]}" == "$newmaster" ]; then
        local_cleanup
        print_result "$0 Wrong slave has been promoted!!!" $PRM_FAIL
    fi
    
    # So, slaves[0] is still a slave, does it have the row?
    cnt=`echo 'select count(*) from test.test_prm_binlog_parser where id=1;' | ${uname_ssh[${slaves[0]}]} "sudo $MYSQL -BN"`

    if [ "$cnt" -ne 1 ]; then
        local_cleanup
        print_result "$0 The row is missing on the slave!!!" $PRM_FAIL
    fi

    local_cleanup	   
    print_result "$0" $PRM_SUCCESS

}

local_cleanup() {

    ${uname_ssh[$master]} "sudo iptables -F OUTPUT"
    ${uname_ssh[$master]} "sudo service corosync start; sleep 3; sudo service pacemaker start"

    master=`check_master` # needs to be after the above 2 commands

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
