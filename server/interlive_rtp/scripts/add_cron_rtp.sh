#!/bin/sh
target=$1
cmd="/opt/interlive_rtp/$target/scripts/check_service.sh $target"
job='* * * * * '$cmd
base=`crontab -l | grep -v "$cmd"`
echo "$base">/var/spool/cron/root
echo "$job">>/var/spool/cron/root
