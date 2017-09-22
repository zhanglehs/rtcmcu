#!/bin/sh

cd /home/shp2p/interlive/server/portal/live_center/testcase

date

for f in test_*.py
do
	echo "processing $f ..."
	/usr/local/bin/python $f
done


