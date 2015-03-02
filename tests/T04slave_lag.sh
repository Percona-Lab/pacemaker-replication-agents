#!/bin/bash
#
# Test description
# 
# Test how slave lag affects the reader vips, max_slave_lag must be greater 
# than 10s.
# This test also verifies how the config reacts to the readable attribute being set
# to 0.

# global include files
. config
. functions

testdir=`dirname $0`


#Called by other tests to get PRM up
setup() {

allsetup

} 

runtest() {
    local sleep_time
    local master
    local sql

    build_assoc_uname_ssh
    master=`check_master`
    max_slave_lag=`get_max_slave_lag`

    #sleep time will be twice max_slave_lag+1
    sleep_time=$((${max_slave_lag}*2+1))

    # Check rvip running on master (should not all be on master)
    check_slaves $master
    check_VIPs -u $master
    if [ "$?" -gt 0 ]; then
        print_result "$0" $PRM_OTHER
    fi

    # run blocking sql, need SBR 

    (cat <<EOF
create database if not exists test;
use test;
drop table if exists test_prm_slave_lag; 
create table test_prm_slave_lag (
    id int NOT NULL AUTO_INCREMENT,
    data int,
    primary key(id));
/*!50100 set global binlog_format='STATEMENT' */;
insert into test_prm_slave_lag (data) select sleep(${sleep_time});
drop table test_prm_slave_lag;

EOF
    ) | ${uname_ssh[$master]} "sudo $MYSQL"

    # sleep max_slave_lag + 5 (assuming slave monitor interval is 2s
    sleep $max_slave_lag 
    sleep 5

    # Check rvip running on master (should all be on master)
    check_slaves $master
    check_VIPs -u $master > /dev/null
    if [ "$?" -eq 0 ]; then
        print_result "$0 VIPs are not all on the master" $PRM_FAIL
    fi

    # sleep max_slave_lag + 5
    sleep $max_slave_lag 
    sleep 5

    # Check rvip running on master (should not all be on master)
    check_slaves $master
    check_VIPs -u $master
    rc=$?
    if [ "$rc" -gt 0 ]; then
        print_result "$0 VIPs have not been well distributed after the slave lag" $PRM_FAIL
    else
        print_result "$0" $PRM_SUCCESS
    fi

}

#Called by other test to get PRM down
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
