#!/bin/sh
cwd=$(pwd)
src_path=$(dirname $cwd)
#python_bin='~/bin/python'
python_bin=python

copy_files() {
	local src_path dest_path conf_file
	src_path=$1
	dest_path=$2
	conf_file=$3

	cd $src_path
	mkdir -p $dest_path
	cp -f $src_path/lctrl.sh $dest_path
	cp -f $src_path/monitor.sh $dest_path
	cp -f $src_path/$conf_file $dest_path/lctrl.conf
	cp -f $src_path/*.py $dest_path
	cd -
}


deploy() {
	local src_path dest_path conf_file
	src_path=$1
	dest_path=$2
	conf_file=$3

	$python_bin $dest_path/lctrl.py stop
	sleep 1

	copy_files $src_path $dest_path $conf_file

	cd $dest_path
	$python_bin $dest_path/lctrl.py start
	cd -
}


deploy $src_path '/opt/interlive/lctrl' 'lctrl.conf'
#deploy $src_path '/opt/interlive/lctrl2' 'lctrl2.conf'
