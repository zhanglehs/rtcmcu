#!/bin/sh
target=$1
( crontab -l | grep -v /opt/interlive/$target/scripts/check_service.sh )| crontab -

