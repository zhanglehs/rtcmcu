#!/bin/sh

source /etc/profile

status=`service syslog-ng status | grep running | wc -l`

if [[ "$status" == 0 ]]
then
        service syslog-ng restart
fi
