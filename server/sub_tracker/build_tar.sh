#!/bin/sh
if [ $# == 0 ]; then
        build_type="debug"
        version_build="snapshot"
elif [ $# == 1 ]; then
        build_type=$1
		version_build="snapshot"
else
        build_type=$1
        version_build=$2
fi;



version_base="2.3"
version_git_branch=`git rev-parse --abbrev-ref HEAD`
version_git_sha=`git rev-list HEAD | head -n 1`
version_date=`date '+%y%m%d'`
version_date_full=`date`
version_file=version.h
version="$version_base.$version_date.$version_build"


if [[ $version_git_branch == "" ]]; then
    echo "a git repos is required to get version info, exiting..."
    exit -1
fi

touch $version_file
echo "#define TRACKER_VERSION \"$version\"" > $version_file
echo "#define TRACKER_VERSION_BUILD \"$build_type\"" >> $version_file
echo "#define TRACKER_VERSION_GIT_BRANCH  \"$version_git_branch\"" >> $version_file
echo "#define TRACKER_VERSION_GIT_SHA    \"$version_git_sha\"" >> $version_file
echo "#define TRACKER_VERSION_DATE    \"$version_date_full\"" >> $version_file

echo "generate version number successfully."

make clean
if [[ $build_type == "release" ]]; then
make release=1
else
make
fi

retval=$?
if [ $retval -ne 0 ]; then
    echo "make failed"
    exit -1
fi

tar zcvf "tracker-$version.tar.gz" tracker tracker.xml tracker.sh
