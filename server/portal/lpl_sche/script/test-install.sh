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
rm $NGINX_PATH/conf/nginx.test.conf -rf
rm $NGINX_PATH/conf/region_group.xml.test -rf

# copy files
mkdir $NGINX_PATH/conf/data
cd ../
cp *.lua $NGINX_PATH/conf
cp *.ini $NGINX_PATH/conf
cp *.xml $NGINX_PATH/conf
cp nginx.conf $NGINX_PATH/conf/nginx.conf
cp data/* $NGINX_PATH/conf/data

rm $NGINX_PATH/conf/self_rules.xml -r
rm $NGINX_PATH/conf/region_group.xml -rf
rm $NGINX_PATH/conf/nginx.conf -rf
cp self_rules.xml.test $NGINX_PATH/conf/self_rules.xml
cp region_group.xml.test $NGINX_PATH/conf/region_group.xml
cp rtp_region_group.xml.test $NGINX_PATH/conf/rtp_region_group.xml
cp nginx.test.conf $NGINX_PATH/conf/nginx.conf

chmod a+w $NGINX_PATH/conf/self_rules.xml

sudo $NGINX_PATH/sbin/nginx -s reload
cd -
