#!/bin/bash

. config
. functions

total=0
success=0
failed=0

testdir=`dirname $0`

allcleanup
allsetup

for t in `ls T*.gtid.sh T*.all.sh`
do
	let total=total+1
	./$t runtest
	if [ "$?" -eq "$PRM_SUCCESS" ]; then
		let success=success+1
	else
		let failed=failed+1
	fi
	allcleanup
	allsetup
	sleep 10
done

echo "Total: $total, Success: $success, Failed: $failed"
