1. 代码
git上提交代码，并打好tag。
将代码更新到/opt/rpmbuild/SOURCES/src/live_stream_svr。
（tag格式为：rtp-3.3.年月日.count，例如rtp-3.3.170424.1）

2. 编译并打包receiver和forward
cd /opt/rpmbuild
./build_receiver_rtp.sh 3.3.170424.1

3. 部署
cd /root
vi dep-rpm.sh
修改NEWVER
./dep-rpm.sh