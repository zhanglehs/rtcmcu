#!/bin/sh

cwd=`pwd`
src_path=`dirname $cwd`
tar_path=`dirname $src_path`
tar_name=`basename $src_path`

build_date=`date +\%Y\%m\%d_%H%M%S`
tar_pack_name="`basename $src_path`_$build_date.tar.gz"

echo "src_path:$src_path"
echo "tar_path:${tar_path}"
echo "pack_name:${tar_pack_name}"
common_dir_src="${tar_path}/common"
common_dir_dst="${src_path}/common" 
echo "common_dir_src:$common_dir_src"
echo "common_dir_dst:$common_dir_dst"


mkdir ${common_dir_dst}
cp -f ${common_dir_src}/*.py ${common_dir_dst} 
cd $tar_path
tar -zcvf $tar_path/$tar_pack_name ./$tar_name/*.py ./$tar_name/common/*.py ./$tar_name/lup_sche.xml ./$tar_name/lup_sche.sh ./$tar_name/monitor.sh ./$tar_name/script/install.sh
rm -rf ${common_dir_dst}
mv $tar_path/$tar_pack_name ../ 

cd -
