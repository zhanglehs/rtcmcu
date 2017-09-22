if [ $# == 0 ]; then
	echo "error! please add version";
	exit;
fi;

version=$1
cd SOURCES/src
sh make_tar_live.sh receiver $version
cd ../..
cd SPECS
sh set_version.sh $version receiver.spec
rpmbuild -ba receiver.spec
