#!/bin/bash

. config

total=0
success=0
failed=0

for t in `ls ./T*.sh`
do
	$t setup
	let total=total+1
	$t runtest
	if [ "$?" -eq "$PRM_SUCCESS" ]; then
		let success=success+1
	else
		let failed=failed+1
	fi
	$t cleanup
done

echo "Total: $total, Success: $success, Failed: $failed"
