
conf_file=receiver-test.xml

sed -i "s/listen_port=\"80\"/listen_port=\"8101\"/g" $conf_file 
sed -i "s/listen_port=\"8080\"/listen_port=\"8141\"/g" $conf_file
sed -i "s/backend_listen_port=\"2935\"/backend_listen_port=\"8501\"/g" $conf_file
sed -i "s/listen_port=\"2080\"/listen_port=\"8541\"/g" $conf_file
sed -i "s/listen_port=\"81\"/listen_port=\"8581\"/g" $conf_file
sed -i "s/listen_port=\"2935\"/listen_port=\"8501\"/g" $conf_file
sed -i "s/listen_port=\"2936\"/listen_port=\"8281\"/g" $conf_file
