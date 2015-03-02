#!/bin/bash
#
# Test description
# 
# Test a crash of mysqld (kill -9) on the master, restart should be on the same host
# and should stay master.  A second kill -9 within 1h, the master role should fail 
# over to another node
#


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

    #kill mysqld
    runcmd "${uname_ssh[$master1]}" 'kill -9 `pidof mysqld`'

    #wait a bit
    sleep 20

    # Should have been restarted in place
    master2=`check_master`
    if [ "$master1" != "$master2" ]; then
        print_result "$0" $PRM_FAIL
    fi

    #Now, a second kill 
    #kill mysqld
    runcmd "${uname_ssh[$master1]}" 'kill -9 `pidof mysqld`'

    #wait a bit
    sleep 20

    # Should have failed over
    master2=`check_master`
    if [ "$master1" = "$master2" ]; then
    	local_cleanup
        print_result "$0 Same master" $PRM_FAIL
    fi

    check_slaves $master2
    rc=$?    
    if [ "$rc" -ne "$PRM_SUCCESS" ]; then
    	local_cleanup
        print_result "$0 Issue with slaves" $PRM_FAIL
    fi

    check_VIPs -u $master2
    rc=$?    
    local_cleanup

    if [ "$rc" -ne "$PRM_SUCCESS" ]; then
        print_result "$0 Issue with VIPs" $PRM_FAIL
    else
        print_result "$0" $PRM_SUCCESS
    fi


}

local_cleanup() {

    #Clean the resource
    runcmd "${uname_ssh[$master1]}"  "crm_attribute --node $master1 --name p_mysql_last_crash -l reboot --delete" > /dev/null
    ( runcmd "${uname_ssh[$master1]}" 'crm resource cleanup p_mysql' ) > /dev/null 2> /dev/null
    sleep 10

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
