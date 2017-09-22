#! /bin/sh
#config
ROOT=$(pwd)
CUR_DATE=`date +%Y%m%d%H%M%S`
PRJ_GIT_URL=ssh://rpmbuild@10.10.72.167:22022/home/git/mirror_live_transcoder.git
#PRJ_GIT_NAME=$PRJ_GIT_URL | awk -F'/' '{print $(NF)}' | awk -F'.' '{print $(NF-1)}'
PRJ_GIT_BRAHCH=release/2.6
PRJ_BIN_NAME=ffmpeg
PRJ_LOCAL_PATH=$ROOT/SOURCES/src/mirror_live_transcoder/dist/bin/
FTP_SERVER_IP=10.10.72.167
FTP_SERVER_USR=anonymous
FTP_SERVER_PWD=anonymous
FTP_SERVER_SUBDIR=pub/ffmpeg
PRJ_GIT_NAME=`echo $PRJ_GIT_URL | awk -F'/' '{print $(NF)}' | awk -F'.' '{print $(NF-1)}'`
DIST_PATH=$ROOT/RPMS/x86_64/ffmpeg/


#main proc
echo $PRJ_LOCAL_PATH
echo $DIST_PATH
cd SOURCES/src
git clone $PRJ_GIT_URL
cd $PRJ_GIT_NAME
git pull --rebase
git checkout $PRJ_GIT_BRAHCH
make
#echo "cp $PRJ_LOCAL_PATH$PRJ_BIN_NAME $PRJ_LOCAL_PATH$PRJ_BIN_NAME$CUR_DATE"

if [ ! -d "$DIST_PATH" ]; then
  mkdir $DIST_PATH
  fi

  cp -f $PRJ_LOCAL_PATH$PRJ_BIN_NAME $DIST_PATH$PRJ_BIN_NAME-$CUR_DATE
  cp -f $PRJ_LOCAL_PATH$PRJ_BIN_NAME $DIST_PATH$PRJ_BIN_NAME




##ftp start
#ftp -i -n << FTPIT
#open $FTP_SERVER_IP
#user $FTP_SERVER_USR $FTP_SERVER_PWD
#passive
#binary
#cd $FTP_SERVER_SUBDIR
#lcd $PRJ_LOCAL_PATH
#delete $PRJ_BIN_NAME
#put $PRJ_BIN_NAME
#put $PRJ_BIN_NAME-$CUR_DATE
#ls
#quit
#FTPIT
##ftp end

  rm -f $PRJ_LOCAL_PATH$PRJ_BIN_NAME-$CUR_DATE
