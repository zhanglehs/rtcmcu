NGINX_PATH=/opt/interlive/nginx

# clean up
rm $NGINX_PATH/conf/*.lua -rf
rm $NGINX_PATH/conf/*.xml -rf
rm $NGINX_PATH/conf/nginx.conf -rf

# copy files
cd ../
cp *.lua $NGINX_PATH/conf
cp nginx.conf $NGINX_PATH/conf/nginx.conf
cp *.xml $NGINX_PATH/conf

sudo $NGINX_PATH/sbin/nginx -s reload
cd -
