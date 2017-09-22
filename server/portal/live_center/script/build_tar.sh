#!/bin/sh

cwd=$(pwd)
src_path=$(dirname $cwd)
tar_path=$(dirname $src_path)
tar_name=$(basename $src_path)
cd $tar_path
cp -f $src_path/lctrl.conf.sample $src_path/lctrl.conf
tar -zcf $tar_path/$tar_name.tar.gz ./$tar_name/*.py ./$tar_name/lctrl.conf ./$tar_name/lctrl.sh ./$tar_name/monitor.sh

cd -
