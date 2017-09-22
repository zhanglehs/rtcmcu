
if [ $# == 0 ]; then
	echo "error! please add version";
	exit;
fi;

version=$1
cd SOURCES/src
sh make_tar_live.sh forward $version
sh make_tar_live.sh forward-fp $version
cd ../..
cd SPECS
sh set_version.sh $version forward.spec
rpmbuild -ba forward.spec
#cd SOURCES
#sh make_tar_fp.sh $version
#cd ..
sh set_version.sh $version forward-fp.spec
rpmbuild -ba forward-fp.spec
