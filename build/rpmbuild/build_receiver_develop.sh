# ./build_receiver_develop.sh 3.1.150924.1 dev_rtp

if [ $# != 2 ]; then
	echo "error! please add version";
	exit;
fi;

set -v on

version=$1
git_branch=$2
cd SOURCES/src
sh make_tar_live_develop.sh receiver $version $git_branch
cd ../..
cd SPECS
sh set_version_develop.sh $git_branch $version receiver_develop.spec
rpmbuild -ba receiver_develop.spec
