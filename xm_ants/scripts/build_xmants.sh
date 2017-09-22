rand=$RANDOM
export tmp_version_file=.tmp_version-$rand.tmp
cd SOURCES/xm_ants
sh make_tar_xmants.sh
version=`cat $tmp_version_file`
rm -f $tmp_version_file
cd ../../SPECS
sh set_version.sh $version xm_ants.spec
rpmbuild -ba xm_ants.spec

