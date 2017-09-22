#!/bin/bash
# set file path
NGINX_ACCESS_LOG=/opt/logs/reverse_proxy/access.log
NGINX_ERROR_LOG=/opt/logs/reverse_proxy/error.log

# rename log
mv $NGINX_ACCESS_LOG $NGINX_ACCESS_LOG.`date -d yesterday +%Y%m%d`
mv $NGINX_ERROR_LOG $NGINX_ERROR_LOG.`date -d yesterday +%Y%m%d`

touch $NGINX_ACCESS_LOG
touch $NGINX_ERROR_LOG
# restart nginx
kill -USR1 $(cat "/opt/interlive/nginx/logs/nginx.pid")

