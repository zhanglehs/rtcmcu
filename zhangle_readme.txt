一、编译
1. 流程
	$ cd ./third_pary
	$ make
	$ cd ../server/interlive_rtp
	$ make
2. 环境
jsonc依赖环境:
	gcc (or another C compiler)
	libtool
	autoconf (autoreconf)
	automake
protobuf依赖环境:
	autoconf
	automake
	libtool
	curl (used to download gmock)
	make
	g++
	unzip
在我的ubuntu上执行以下命令就可编译通过：
sudo apt-get install autoconf
sudo apt-get install libtool



二、app说明
1. ./server/interlive_rtp/receiver_rtp
	允许uploader上传，以及player播放



三、运行
$ cd ./server/interlive_rtp
$ ./receiver_rtp -c receiver_rtp_tt2.xml -D
$ ./receiver_rtp -c receiver_rtp_tt.xml -D
(-D选项仅用于调试，即不以deamon模式运行）