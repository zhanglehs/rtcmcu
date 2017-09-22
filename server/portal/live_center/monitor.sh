#!/bin/sh

BASE_PATH=/opt/interlive/lctrl
LOG_PATH=/opt/logs/lctrl

cd $BASE_PATH

status=`sh ./lctrl.sh status|grep stop|wc -l`
if [[ "$status" == 1 ]]
then
	echo `date` >> $LOG_PATH/restart.log
	echo 'start lctrl by monitor.sh'  >> $LOG_PATH/restart.log
	sh ./lctrl.sh start
fi
