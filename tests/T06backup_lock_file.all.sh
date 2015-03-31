#!/bin/bash
#
# Test description
# 
# Test the behavior with the presence of a backup lock file that is there to 
# Prevent slave restart during a backup

# global include files
. config
. functions

testdir=`dirname $0`


setup() {

allsetup

} 

runtest() {
    local master
    local readable_state

    build_assoc_uname_ssh
    master=`check_master`

    # Check rvip running on master (should not all be on master)
    check_slaves $master

    #Now we pick a slave from
    declare -a slaves=( `crm_slaves` )

    # On that slave we set a lock file for 15s
    ${uname_ssh[${slave[0]}]} 'sudo flock -xn /var/lock/innobackupex sleep 15' &

    # stop replication
    (cat <<EOF
stop slave;
EOF
    ) | ${uname_ssh[${slave[0]}]} "sudo $MYSQL"

    # wait for a monitor op
    sleep 5

    check_slaves $master

    if [ "${repl_state[${slave[0]}] }" -ne 0 ]; then
        echo "Replication has been restarted on ${slave[0]}"
        print_result "$0" $PRM_FAIL
    else 
        print_result "$0" $PRM_SUCCESS
    fi

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
