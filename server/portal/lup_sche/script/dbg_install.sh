#!/bin/sh
cwd=$(pwd)
src_path=$(dirname $cwd)
dest_path='/opt/interlive/lup_sche'
dest_common_path=${dest_path}/common
mkdir ${dest_common_path} -p

#install source
cd $dest_path
~/bin/python $dest_path/lup_sche.py stop
sleep 3

cp -f $src_path/lup_sche.sh $dest_path
cp -f $src_path/monitor.sh $dest_path
cp -f $src_path/*.py $dest_path
cp -f ${src_path}/common/*.py ${dest_common_path}

cd $dest_path
~/bin/python $dest_path/lup_sche.py start

#install crontab
base_shell="$dest_path""/monitor.sh"
cmd="sh $base_shell"" > /dev/null 2>&1"
job=`echo "* * * * * "$cmd`
base=`crontab -l | grep -v "$base_shell"`
base="$base""\n""$job"
echo -e "$base" | crontab -
crontab -l

cd -
