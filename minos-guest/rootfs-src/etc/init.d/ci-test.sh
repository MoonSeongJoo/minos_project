#!/bin/sh

TEST_RESULT=0
#
# entry point for ci-testing
#
# test1.sh || TEST_RESULT=1
# test2.sh || TEST_RESULT=1
# .....
#

if [ -e /sys/kernel/qemu_exit/code ]; then
	echo $TEST_RESULT > /sys/kernel/qemu_exit/code
else
	echo "no exit node found for the test result.."
fi


