#!/bin/sh

if [ $# != 1 ]; then
	echo -e "\033[31m[ERROR]\033[0m no config file."
	echo "Usage: ./deploy_interlive test_env2.conf"
	exit 1	
fi

source $1

function replace_lctrl()
{
	echo -e "\033[32m[INFO]\033[0mreplace $1 lctrl to $2:$3"
	python ./replace_lctrl.py $1 $2 $3
}

function replace_tracker()
{
	echo -e "\033[32m[INFO]\033[0mreplace $1 tracker to $2:$3"
	python ./replace_tracker.py $1 $2 $3	
}

function copy_role()
{
	ip=$1
	role=$2
	role_dir=$3

	echo -e "\033[32m[INFO]\033[0mdeploy $role to $ip"
}

rm receiver-test.xml forward-fp-test.xml forward-nfp-test.xml -rf

cp $src_dir/receiver-test.xml .
replace_lctrl receiver-test.xml $lctrl_ip $lctrl_port
replace_tracker receiver-test.xml $tracker_ip $tracker_port

cp $src_dir/forward-fp-test.xml .
replace_lctrl forward-fp-test.xml $lctrl_ip $lctrl_port
replace_tracker forward-fp-test.xml $tracker_ip $tracker_port

cp $src_dir/forward-nfp-test.xml .
replace_lctrl forward-nfp-test.xml $lctrl_ip $lctrl_port
replace_tracker forward-nfp-test.xml $tracker_ip $tracker_port




for i in ${receiver_list[@]}
do
	echo -e "\033[32m[INFO]\033[0mdeploy receiver to $i"
	ssh root@$i "killall -9 receiver"
	ssh root@$i "rm -f $receiver_dst_dir/receiver"
	ssh root@$i "mkdir -p $receiver_dst_dir"
	ssh root@$i "mkdir -p /opt/logs/interlive/receiver"
	scp $src_dir/crossdomain.xml root@$i:$receiver_dst_dir/
	scp $src_dir/log4cplus.cfg root@$i:$receiver_dst_dir/
	scp $src_dir/receiver root@$i:$receiver_dst_dir/
	scp receiver-test.xml root@$i:$receiver_dst_dir/
	scp -r $src_dir/scripts root@$i:$receiver_dst_dir/
done

for i in ${fp_list[@]}
do
	echo -e "\033[32m[INFO]\033[0mdeploy forward-fp to $i"
	ssh root@$i "killall forward"
	ssh root@$i "rm -f $fp_dst_dir/forward"
	ssh root@$i "mkdir -p $fp_dst_dir"
	ssh root@$i "mkdir -p /opt/logs/interlive/forward-fp"
	scp $src_dir/crossdomain.xml root@$i:$fp_dst_dir/
	scp $src_dir/forward root@$i:$fp_dst_dir/
	scp forward-fp-test.xml root@$i:$fp_dst_dir/
done

for i in ${nfp_list[@]}
do
	echo -e "\033[32m[INFO]\033[0mdeploy forward-nfp to $i"
	ssh root@$i "killall forward"
	ssh root@$i "rm -f $nfp_dst_dir/forward"
	ssh root@$i "mkdir -p $nfp_dst_dir"
	ssh root@$i "mkdir -p /opt/logs/interlive/forward"
	scp $src_dir/crossdomain.xml root@$i:$nfp_dst_dir/
	scp $src_dir/forward root@$i:$nfp_dst_dir/
	scp forward-nfp-test.xml root@$i:$nfp_dst_dir/
done

#start new process
for i in ${receiver_list[@]}
do
	echo -e "\033[32m[INFO]\033[0mstarting receiver on $i"
	ssh root@$i "cd $receiver_dst_dir; scripts/replace_port.sh"
	ssh root@$i "cd $receiver_dst_dir; $receiver_dst_dir/receiver -c $receiver_dst_dir/receiver-test.xml"
done

for i in ${fp_list[@]}
do
	echo -e "\033[32m[INFO]\033[0mstarting forward-fp on $i"
	ssh root@$i "cd $fp_dst_dir; $fp_dst_dir/forward -c $fp_dst_dir/forward-fp-test.xml"
done

for i in ${nfp_list[@]}
do
	echo -e "\033[32m[INFO]\033[0mstarting forward-nfp on $i"
	ssh root@$i "cd $nfp_dst_dir; $nfp_dst_dir/forward -c $nfp_dst_dir/forward-nfp-test.xml"
done

