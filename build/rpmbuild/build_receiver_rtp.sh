if [ $# == 0 ]; then
	echo "error! please add version";
	exit;
fi;

version=$1
cd SOURCES/src
sh make_tar_live_rtp.sh receiver_rtp $version ""
cd ../..
cd SPECS
sh set_version.sh $version receiver_rtp.spec
rpmbuild -ba receiver_rtp.spec
