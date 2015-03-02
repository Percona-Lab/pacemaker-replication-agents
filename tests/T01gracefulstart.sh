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

	allsetup
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












































































































































