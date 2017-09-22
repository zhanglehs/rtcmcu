NGINX_PATH=/opt/interlive/nginx

# clean up
rm $NGINX_PATH/conf/data -rf
rm $NGINX_PATH/conf/testcase -rf
rm $NGINX_PATH/conf/*.lua -rf
rm $NGINX_PATH/conf/region_group* -rf
rm $NGINX_PATH/conf/*.sh -rf
rm $NGINX_PATH/conf/ip_region.ini -rf
rm $NGINX_PATH/conf/*.xml -rf
rm $NGINX_PATH/conf/region -rf
rm $NGINX_PATH/conf/nginx.conf -rf

# copy files
mkdir $NGINX_PATH/conf/data
cd ../
cp *.lua $NGINX_PATH/conf
cp *.ini $NGINX_PATH/conf
cp *.xml $NGINX_PATH/conf
cp nginx.conf $NGINX_PATH/conf/nginx.conf
cp data/* $NGINX_PATH/conf/data

chmod a+w $NGINX_PATH/conf/self_rules.xml

sudo $NGINX_PATH/sbin/nginx -s reload
cd -
