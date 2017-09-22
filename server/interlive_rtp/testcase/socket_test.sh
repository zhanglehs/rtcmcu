#/bin/sh

cd  /home/interlive/test_dir/server/interlive/
make cleanall
make forwardtest
/home/interlive/test_dir/server/interlive/forwardtest -c testcase/forwardtest.xml
sleep 5
python /home/interlive/test_dir/server/interlive/testcase/socket_test.py
sleep 110
killall -SIGINT /home/interlive/test_dir/server/interlive/forwardtest
make lcov
cp -rf fresults/ /home/interlive/nginx/openresty/nginx/html
