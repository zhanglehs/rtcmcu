一、编译
$ cd ./third_pary
$ make
$ cd ../server/interlive_rtp
$ make

二、app说明
1. ./server/interlive_rtp/forward_rtp
	允许uploader上传，以及player播放

2. ./server/interlive_rtp/receiver_rtp
	只允计player播放
	
三、运行
$ cd ./server/interlive_rtp
$ ./forward_rtp -c receiver_rtp_tt2.xml -D
$ ./forward_rtp -c receiver_rtp_tt.xml -D
(-D选项仅用于调试，即不以deamon模式运行）