#!/bin/sh

cd ~/interlive/server/portal/live_center/lup_sche/testcase

date

for f in test_*.py
do
	echo "processing $f ..."
	/usr/local/bin/python $f
done

