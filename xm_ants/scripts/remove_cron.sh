#!/bin/sh
( crontab -l | grep -v /opt/interlive/xm_ants/scripts/check_service.sh )| crontab -
( crontab -l | grep -v /opt/interlive/xm_ants/scripts/clear_log.sh )| crontab -
