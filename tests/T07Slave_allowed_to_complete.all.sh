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

allsetup

} 

runtest() {
    local master newmaster
    local count_result

    build_assoc_uname_ssh
    master=`check_master`

    declare -a slaves=( `crm_slaves` )
    #throttle iops on the slaves
    throttle_iops ${slaves[0]} 10
    throttle_iops ${slaves[1]} 10

    # Generate iops so the slave will lag behind
    (cat <<EOF
create database if not exists test;
use test;
drop table if exists test_prm_slave_complete; 
create table test_prm_slave_complete (
    data varchar(36),
    datamd5 binary(32),
    datasha1 binary(40),
    primary key (data,datamd5,datasha1),
    unique key idx_data (datamd5,data),
    unique key idx_datamd5 (datasha1,datamd5),
    unique key idx_datasha1 (datasha1,data)
    ) engine=innodb;
set global innodb_flush_neighbors=0;
insert into test_prm_slave_complete (data,datamd5,datasha1) values (uuid(),md5(uuid()),sha1(uuid()));
insert into test_prm_slave_complete (data,datamd5,datasha1) select uuid(),md5(uuid()),sha1(uuid()) from test_prm_slave_complete;
insert into test_prm_slave_complete (data,datamd5,datasha1) select uuid(),md5(uuid()),sha1(uuid()) from test_prm_slave_complete;
insert into test_prm_slave_complete (data,datamd5,datasha1) select uuid(),md5(uuid()),sha1(uuid()) from test_prm_slave_complete;
insert into test_prm_slave_complete (data,datamd5,datasha1) select uuid(),md5(uuid()),sha1(uuid()) from test_prm_slave_complete;
insert into test_prm_slave_complete (data,datamd5,datasha1) select uuid(),md5(uuid()),sha1(uuid()) from test_prm_slave_complete;
insert into test_prm_slave_complete (data,datamd5,datasha1) select uuid(),md5(uuid()),sha1(uuid()) from test_prm_slave_complete;
insert into test_prm_slave_complete (data,datamd5,datasha1) select uuid(),md5(uuid()),sha1(uuid()) from test_prm_slave_complete;
insert into test_prm_slave_complete (data,datamd5,datasha1) select uuid(),md5(uuid()),sha1(uuid()) from test_prm_slave_complete;
insert into test_prm_slave_complete (data,datamd5,datasha1) select uuid(),md5(uuid()),sha1(uuid()) from test_prm_slave_complete;
insert into test_prm_slave_complete (data,datamd5,datasha1) select uuid(),md5(uuid()),sha1(uuid()) from test_prm_slave_complete;
insert into test_prm_slave_complete (data,datamd5,datasha1) select uuid(),md5(uuid()),sha1(uuid()) from test_prm_slave_complete;
insert into test_prm_slave_complete (data,datamd5,datasha1) select uuid(),md5(uuid()),sha1(uuid()) from test_prm_slave_complete;
EOF
    ) | ${uname_ssh[$master]} "sudo $MYSQL"

    # Now, the slave are stuck applying the inserts, put the master in standby
    ${uname_ssh[$master]} "sudo crm node standby $master"
    sleep 5

    newmaster=`check_master `
    # Sanity check
    if [ "$newmaster" != "No master has been promoted" ]; then
        local_cleanup
        print_result "$0 A master has been promoted too early" $PRM_FAIL
    fi

    #unthrottle iops on the slaves
    throttle_iops ${slaves[0]} 10000
    throttle_iops ${slaves[1]} 10000

    # wait for the sleep to complete and promotion
    sleep 15

    newmaster=`check_master `
    # Sanity check
    if [ "$newmaster" = "No master has been promoted" ]; then
	local_cleanup
        print_result "$0 No master has been promoted" $PRM_FAIL
    fi

    if [ "$master" = "$newmaster" ]; then
	local_cleanup
        print_result "$0 Still the same master" $PRM_FAIL
    fi

    count_result=`(cat <<EOF
use test;
select count(*) from test_prm_slave_complete;
EOF
    ) | ${uname_ssh[$newmaster]} "sudo $MYSQL -N"`

    local_cleanup
    if [ "$count_result" -ne 4096 ]; then
        print_result "$0 missing rows in test_prm_slave_complete, reporte $count_result" $PRM_FAIL
    else 
        print_result "$0" $PRM_SUCCESS
    fi

}

local_cleanup() {

    #unthrottle iops on the slaves
    throttle_iops ${slaves[0]} 10000
    throttle_iops ${slaves[1]} 10000

    online_all

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
