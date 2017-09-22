#!/bin/sh

if [ $# -lt 3 ]; then
	echo "error! please add module and version";
	echo "e.g. make_tar_live_develop.sh receiver 3.1.150924.1 dev_rtp";
	exit;
fi;

set -v on

# version is like 3.1.140915.1 
version=$2
git_branch=$3

# module means receiver/xm_ants/forward-fp/forward
module=$1 
dst=$module-$git_branch-$version

echo $dst

git clone ssh://rpmbuild@10.10.72.167:22022/home/git/mirror_live_stream_svr.git live_stream_svr
cd live_stream_svr
git checkout $git_branch
git pull

./generate_version.sh release $version
git archive -o $dst.tar HEAD
cd ..

mv live_stream_svr/$dst.tar .
mkdir $dst
tar vxf $dst.tar -C $dst
rm $dst.tar -rf

cp $dst/build/$module.mk $dst/build/release.mk -rf

cp live_stream_svr/lib/version.h $dst/lib/version.h 
tar -cvzf $dst.tar.gz $dst
mv $dst.tar.gz ../$dst.tar.gz
