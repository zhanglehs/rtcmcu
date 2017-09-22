#!/bin/sh

service=$1
status=`/sbin/service $service status`
if [[ "$status" == *stopped* ]]
then
	#echo `date`
	#echo 'service $service stopped , restart'
	ulimit -c 0
	/sbin/service $service start
fi	
