#!/bin/bash
#
# Test description
# This test is just a sanity test for a normal start.  The expected results
# are a master been promoted, slaves replicating from the master and VIPs 
# enabled.

# global include files
. config
. functions

testdir=`dirname $0`


#Called by other tests to get PRM up
setup() {

# restart corosync
runcmd_all /etc/init.d/corosync start 1> /dev/null
sleep 10

# start pacemaker
runcmd_all /etc/init.d/pacemaker start 1> /dev/null
sleep 20


#load configuration
cat $testdir/base_config.crm | runcmd $SSH1 'cat - > /tmp/config.crm'
runcmd $SSH1 'crm configure load update /tmp/config.crm'

online_all

sleep 10

} 

runtest() {
    local rc

    build_assoc_uname_ssh
    master=`check_master`
    rc=$?
    
    if [ "$rc" -ne "$PRM_SUCCESS" ]; then
        print_result "$0" $PRM_FAIL
    fi
    
    check_slaves $master
    rc=$?    
    if [ "$rc" -ne "$PRM_SUCCESS" ]; then
        print_result "$0" $PRM_FAIL
    fi
    

    check_VIPs -u $master
    rc=$?    
    if [ "$rc" -ne "$PRM_SUCCESS" ]; then
        print_result "$0" $PRM_FAIL
    else
        print_result "$0" $PRM_SUCCESS
    fi

}

#Called by other test to get PRM down
cleanup() {

    # stop pacemaker
    runcmd_all /etc/init.d/pacemaker stop 1> /dev/null
    sleep 10

    # stop pacemaker
    runcmd_all /etc/init.d/corosync stop 1> /dev/null
    sleep 10

    runcmd_all 'rm -f /var/lib/heartbeat/crm/*' 1> /dev/null
    
}


# What kind of method was invoked?
case "$1" in
  setup)    setup;;
  runtest)  runtest;;
  cleanup)  cleanup;;

 *)     echo 'Non implemented'
        exit 1
esac












































































































































