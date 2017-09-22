#!/bin/sh

git clone ssh://gforge.1verge.net:22022/gitroot/live_stream_svr

cd live_stream_svr
git checkout flv_ant
. ./generate_version.sh
version=`get_version_num`
mkdir xm_ants/logs
cd ..
src=xm_ants-$version
mv -f live_stream_svr $src
rm -rf live_stream_svr
tar -cvzf $src.tar.gz $src
mv $src.tar.gz ../
echo $version > $tmp_version_file
