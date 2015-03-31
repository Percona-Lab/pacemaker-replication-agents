#!/bin/bash
#
# Test description
# 
# Test a graceful failover between nodes

# global include files
. config
. functions

testdir=`dirname $0`


#Called by other tests to get PRM up
setup() {

    allsetup

} 

runtest() {

    build_assoc_uname_ssh
    master1=`check_master`

    #Demote/promote
    runcmd "$SSH1" "crm node standby $master1"
    sleep 10

    master2=`check_master`
    rc=$?    
    if [ "$rc" -ne "$PRM_SUCCESS" -o "$master1" = "$master2" ]; then
        echo "check_master failed or same master"
        print_result "$0" $PRM_FAIL
    fi

    check_slaves $master2
    rc=$?    
    if [ "$rc" -ne "$PRM_SUCCESS" ]; then
        echo "check_slaves failed"
        print_result "$0" $PRM_FAIL
    fi

    check_VIPs -u $master2
    rc=$?    
    
    # put the node back online
    runcmd "$SSH1" "crm node online $master1"
    
    if [ "$rc" -ne "$PRM_SUCCESS" ]; then
        print_result "$0" $PRM_FAIL
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
