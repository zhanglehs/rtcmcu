OLDVER='3.3.170508.2'
NEWVER='3.3.171013.2'


RECEIVERS="172.16.1.39"

for   x   in   $RECEIVERS
do
echo ">>>>>>>>>$x receiver"
#ssh root@$x -p 2222 "yum install gcc-c++.x86_64 -y;yum install expat-devel -y;yum install libuuid-devel -y;yum install gperftools -y;"
ssh root@$x  -p 2222 "cd /opt/interlive_rtp/receiver_rtp;cp receiver_rtp.xml receiver_rtp.xml.1 -f;"
ssh root@$x  -p 2222 "rpm -e receiver_rtp;killall receiver_rtp;"
ssh root@$x  -p 2222 "rpm -ivh http://172.16.1.35:7080/receiver_rtp-$NEWVER-1.el6.x86_64.rpm"
ssh root@$x  -p 2222 "cd /opt/interlive_rtp/receiver_rtp;cp receiver_rtp.xml.1 receiver_rtp.xml -f;"
#ssh root@$x -p 2222 "cd /opt/interlive_rtp/receiver_rtp;sed -i \"s/\/opt\/logs/\/opt\/logs1/g\" receiver_rtp.xml"
ssh root@$x  -p 2222 "/usr/local/bin/supervisorctl -c /etc/supervisord.conf restart receiver_rtp"
ssh root@$x  -p 2222 "/usr/local/bin/supervisorctl -c /etc/supervisord.conf start all"
ssh root@$x  -p 2222 "/usr/local/bin/supervisorctl -c /etc/supervisord.conf status"
done

FORWARDFPS="172.16.1.35"

for   x   in   $FORWARDFPS
do
echo ">>>>>>>>>$x foward-fp"
#ssh root@$x -p 2222 "yum install gcc-c++.x86_64 -y;yum install expat-devel -y;yum install libuuid-devel -y;yum install gperftools -y;"
ssh root@$x -p 2222 "cd /opt/interlive_rtp/forward_rtp;cp forward_rtp.xml forward_rtp.xml.1 -f"
ssh root@$x -p 2222 "rpm -e forward-fp_rtp;killall forward_rtp;"
ssh root@$x -p 2222 "rpm -ivh http://172.16.1.35:7080/forward-fp_rtp-$NEWVER-1.el6.x86_64.rpm"
ssh root@$x -p 2222 "cd /opt/interlive_rtp/forward_rtp;cp forward_rtp.xml.1 forward_rtp.xml -f"
#ssh root@$x -p 2222 "cd /opt/interlive_rtp/forward_rtp;sed -i \"s/\/opt\/logs/\/opt\/logs1/g\" forward_rtp.xml"
ssh root@$x -p 2222 "/usr/local/bin/supervisorctl -c /etc/supervisord.conf restart forward_rtp"
ssh root@$x -p 2222 "/usr/local/bin/supervisorctl -c /etc/supervisord.conf start all"
ssh root@$x -p 2222 "/usr/local/bin/supervisorctl -c /etc/supervisord.conf status"
done

FORWARDS="172.16.1.37"

for   x   in   $FORWARDS
do
echo ">>>>>>>>>$x foward"
#ssh root@$x  -p 2222 "yum install gcc-c++.x86_64 -y;yum install expat-devel -y;yum install libuuid-devel -y;yum install gperftools -y;"
ssh root@$x  -p 2222 "cd /opt/interlive_rtp/forward_rtp;cp forward_rtp.xml forward_rtp.xml.1 -f"
ssh root@$x -p 2222 "rpm -e forward_rtp;killall forward_rtp;"
ssh root@$x -p 2222 "rpm -ivh http://172.16.1.35:7080/forward_rtp-$NEWVER-1.el6.x86_64.rpm"
ssh root@$x -p 2222 "cd /opt/interlive_rtp/forward_rtp;cp forward_rtp.xml.1 forward_rtp.xml -f"
#ssh root@$x  -p 2222 "cd /opt/interlive_rtp/forward_rtp;sed -i \"s/\/opt\/logs/\/opt\/logs1/g\" forward_rtp.xml"
ssh root@$x  -p 2222 "/usr/local/bin/supervisorctl -c /etc/supervisord.conf restart forward_rtp"
ssh root@$x  -p 2222 "/usr/local/bin/supervisorctl -c /etc/supervisord.conf start all"
ssh root@$x -p 2222 "/usr/local/bin/supervisorctl -c /etc/supervisord.conf status"
done


#FORWARDS="10.108.27.105 10.108.27.106 10.100.16.156
#for   x   in   $FORWARDS
#do
#echo ">>>>>>>>>$x foward"
#
#ssh root@$x -p 22022 "cd /opt/interlive_rtp/forward_rtp;sed -i \"s/listen_port='80'/listen_port='1080'/g\" forward_rtp.xml"
#ssh root@$x -p 22022 "/usr/local/bin/supervisorctl -c /etc/supervisord.conf start all"
#done