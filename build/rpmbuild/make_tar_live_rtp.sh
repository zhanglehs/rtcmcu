#!/bin/sh

if [ $# -lt 2 ]; then
	echo "error! please add module and version";
	echo "e.g. make_tar_live.sh receiver 3.1.000000.1 ";
	exit;
fi;

# version is like 3.1.140915.1 
version=$2

# module means receiver/xm_ants/forward-fp/forward
module=$1 
#src=live-$version
dst=$module-$version

debug_script=$3

tag_prefix="rtp"

echo $module
if [ "$module" == "xm_ants" ]; then
	tag_prefix="xmants";
fi;

if [ $debug_script != "" ]; then
	cd live_stream_svr
	git checkout flv_ant
	git pull
	./generate_version.sh release $version
	cd ..
	cp live_stream_svr/build/$module.mk live_stream_svr/build/release.mk -rf
	rm $dst -rf
	cp live_stream_svr $dst -rf
	rm $dst.tar -rf
	tar -cvzf $dst.tar.gz $dst
	mv $dst.tar.gz ../$dst.tar.gz
	rm $dst -rf
else
#	git clone ssh://duanbingnan@gforge.1verge.net:22022/gitroot/live_stream_svr
#	git clone ssh://duanbingnan@10.10.72.167:22022/home/git/mirror_live_stream_svr.git live_stream_svr
	
	#git clone ssh://rpmbuild@10.10.72.167:22022/home/git/mirror_live_stream_svr.git live_stream_svr
	cd live_stream_svr
	git reset --hard
	git checkout dev_rtp_only
	#git pull

	./generate_version.sh release $version
	git archive -o $dst.tar $tag_prefix-$version
	cd ..
	
	mv live_stream_svr/$dst.tar .
	mkdir $dst
	tar vxf $dst.tar -C $dst
	rm $dst.tar -rf

	cp $dst/build/$module.mk $dst/build/release.mk -rf
if [ "$module" == "xm_ants" ]; then 
	cp -f ../../RPMS/x86_64/ffmpeg/ffmpeg $dst/bin/ffmpeg
fi	

	cp live_stream_svr/lib/version.h $dst/lib/version.h 
	tar -cvzf $dst.tar.gz $dst
	mv $dst.tar.gz ../$dst.tar.gz
fi
