#!/bin/bash
# set file path
NGINX_ACCESS_LOG=/opt/logs/lpl_sche/access.log

# rename log
mv $NGINX_ACCESS_LOG $NGINX_ACCESS_LOG.`date -d yesterday +%Y%m%d`

touch $NGINX_ACCESS_LOG
# restart nginx
kill -USR1 $(cat "/opt/interlive/nginx/logs/nginx.pid")

