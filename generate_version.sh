#!/bin/bash

if [ $# == 0 ]; then
	version="3.3.000000.0"
	build="debug"
elif [ $# == 1 ]; then
	build=$1
	version="3.1.000000.0"
else
	build=$1
	version=$2
fi;

branch_name=`git rev-parse --abbrev-ref HEAD`

version_branch=$branch_name
version_hash=`git rev-list HEAD | head -n 1`
version_date=`date '+%Y/%m/%d %X %Z'`


version_header_file=lib/version.h

if [[ $branch_name == "" && $version_hash == "" ]]; then
    if [ ! -f $version_header_file ]; then
	    touch $version_header_file
		echo "#define VERSION \"$version\"" > $version_header_file
        echo "#define BUILD   \"$build\"" >> $version_header_file
		echo "#define BRANCH  \"no branch\"" >> $version_header_file
		echo "#define HASH    \"no hash\"" >> $version_header_file
		echo "#define DATE    \"$version_date\"" >> $version_header_file

        echo "git repository not found, create version file, unversion build."
		exit    
	else
	    echo "git repository not found, use existing version file."
	    exit    
	fi  
fi

touch $version_header_file
echo "#define VERSION \"$version\"" > $version_header_file
echo "#define BUILD   \"$build\"" >> $version_header_file
echo "#define BRANCH  \"$version_branch\"" >> $version_header_file
echo "#define HASH    \"$version_hash\"" >> $version_header_file
echo "#define DATE    \"$version_date\"" >> $version_header_file

echo "generate version number successfully."

