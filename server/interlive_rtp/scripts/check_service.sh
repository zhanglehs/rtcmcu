#!/bin/sh

service=$1
status=`/sbin/service $service status`
if [[ "$status" == *stopped* ]]
then
	#echo `date`
	#echo 'service $service stopped , restart'
	/sbin/service $service start
fi
	

