#!/bin/sh

if [ $# != 1 ]; then
	echo -e "\033[31m[ERROR]\033[0m no config file."
	echo "Usage: ./deploy_interlive test_env2.conf"
	exit 1	
fi

source $1

for i in ${receiver_list[@]}
do
	lib_dir=/opt/interlive/lib64
	echo -e "\033[32m[INFO]\033[0mdeploy lib to $i"
	ssh root@$i "mkdir -p $lib_dir; cd $lib_dir; wget http://10.103.14.70/interlive/deploy/interlive_lib.tar.gz; tar zvxf interlive_lib.tar.gz"
	ssh root@$i "echo $lib_dir > /etc/ld.so.conf.d/interlive.conf; ldconfig"
done

for i in ${fp_list[@]}
do
	lib_dir=/opt/interlive/lib64
	echo -e "\033[32m[INFO]\033[0mdeploy lib to $i"
	ssh root@$i "mkdir -p $lib_dir; cd $lib_dir; wget http://10.103.14.70/interlive/deploy/interlive_lib.tar.gz; tar zvxf interlive_lib.tar.gz"
	ssh root@$i "echo $lib_dir > /etc/ld.so.conf.d/interlive.conf; ldconfig"
done

for i in ${nfp_list[@]}
do
	lib_dir=/opt/interlive/lib64
	echo -e "\033[32m[INFO]\033[0mdeploy lib to $i"
	ssh root@$i "mkdir -p $lib_dir; cd $lib_dir; wget http://10.103.14.70/interlive/deploy/interlive_lib.tar.gz; tar zvxf interlive_lib.tar.gz"
	ssh root@$i "echo $lib_dir > /etc/ld.so.conf.d/interlive.conf; ldconfig"
done