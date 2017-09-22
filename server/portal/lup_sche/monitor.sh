#!/bin/sh

BASE_PATH=/opt/interlive/lup_sche
LOG_PATH=/opt/logs/lup_sche

cd $BASE_PATH

status_result=`sh ./lup_sche.sh status`
status=`echo $status_result|grep running|wc -l`
if [[ "$status" == 0 ]]
then
	echo `date` >> $LOG_PATH/restart.log
	echo 'start lup_sche by monitor.sh'  >> $LOG_PATH/restart.log
	sh ./lup_sche.sh start
else
	echo $status_result
fi
