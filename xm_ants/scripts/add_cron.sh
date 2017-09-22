#!/bin/sh
cmd="/opt/interlive/xm_ants/scripts/check_service.sh xm_ants"
job='* * * * * '$cmd
cmd1="/opt/interlive/xm_ants/scripts/clear_log.sh"
job1='0 3 * * 5 '$cmd1
base=`crontab -l | grep -v "$cmd" | grep -v "$cmd1"`
echo "$base">/var/spool/cron/root
echo "$job">>/var/spool/cron/root
echo "$job1">>/var/spool/cron/root
