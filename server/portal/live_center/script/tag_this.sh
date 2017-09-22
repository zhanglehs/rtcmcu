#!/bin/sh

svn_tag_root="http://gforge.1verge.net/svn/interlive/tags/lctrl"
cwd=$(pwd)
src_path=$(dirname $cwd)
new_ver_str=

update_version() {
    local major minor third fourth
	major=$(grep -e 'lc_version[ tab]*=' $src_path/lversion.py | sed 's/lc_version[ tab]*=[ tab]*"\([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\)"/\1/')
	minor=$(grep -e 'lc_version[ tab]*=' $src_path/lversion.py | sed 's/lc_version[ tab]*=[ tab]*"\([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\)"/\2/')
	third=$(grep -e 'lc_version[ tab]*=' $src_path/lversion.py | sed 's/lc_version[ tab]*=[ tab]*"\([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\)"/\3/')
	fourth=$(grep -e 'lc_version[ tab]*=' $src_path/lversion.py | sed 's/lc_version[ tab]*=[ tab]*"\([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\)"/\4/')

	new_day=$(date +'%y%m%d')
	if [ $new_day -eq $third ]; then
		fourth=$(expr $fourth + 1)
	elif [ $new_day -gt $third ]; then
		third=$new_day
		fourth=0
	else
		echo "find invalid date $new_day and $third"
		return 3
	fi

	new_ver_str="$major.$minor.$third.$fourth"
	sed "s/lc_version[ tab]*=[ tab]*.*/lc_version = \"$new_ver_str\"/"  $src_path/lversion.py > $src_path/lversion.py.new
	mv -f $src_path/lversion.py.new $src_path/lversion.py

	return 0
}

commit_ver_file() {
	svn commit -m "modify version for $new_ver_str" --force-log $src_path/lversion.py
}

make_tag() {
	tag_name="lctrl_$new_ver_str"
	svn copy $src_path $svn_tag_root/$tag_name -m "add tag for $new_ver_str"
	echo "tag path: $svn_tag_root/$tag_name"
}

while : ; do
	update_version
	[ $? -eq 0 ] || break

	commit_ver_file
	[ $? -eq 0 ] || break

	make_tag
	[ $? -eq 0 ] || break

	break
done

