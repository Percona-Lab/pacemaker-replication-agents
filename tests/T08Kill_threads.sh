#!/bin/bash
#
# Test description
# 
# Test that connections a killed during a demote

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
drop table if exists test_prm_kill_sessions; 
create table test_prm_kill_sessions (
    id int NOT NULL AUTO_INCREMENT,
    data varchar(30),
    data1 int,
    primary key(id)) engine=innodb;
begin;
insert into test_prm_kill_sessions (data,data1) select '${master}', 1;
select sleep(15);
commit;
EOF
    ) | ${uname_ssh[$master]} $MYSQL 2> /dev/null &

    # Now, there's an uncommited transaction on the master, let's demote and promote
    ${uname_ssh[$master]} crm resource demote ${MYSQL_CRM_MS}
    sleep 5
    ${uname_ssh[$master]} crm resource promote ${MYSQL_CRM_MS}
    sleep 5

    # The transaction should have been rollbacked 
    newmaster=`check_master`
    # Sanity check
    if [ "$master" = "$newmaster" ]; then
        echo "Still the same master"
        print_result "$0" $PRM_FAIL
    fi

    count_result=`(cat <<EOF
use test;
select count(*) from test_prm_kill_sessions where data = '$master';
EOF
    ) | ${uname_ssh[$newmaster]} $MYSQL -N`

    #now, a bit of cleanup before ending
    (cat <<EOF
create database if not exists test;
use test;
drop table if exists test_prm_kill_sessions; 
EOF
    ) | ${uname_ssh[$newmaster]} $MYSQL

    if [ "$count_result" -ne 0 ]; then
        echo "Failed to kill existing connections "
        print_result "$0" $PRM_FAIL
    else 
        print_result "$0" $PRM_SUCCESS
    fi

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
