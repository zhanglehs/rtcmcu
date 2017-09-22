#!/bin/sh
#coverage run  --timid ../lctrl.py stop
#REDIS_PATH=software/redis-2.6.14/src
#REDIS_PORT=16397

sleep 3
coverage run -p  --branch ../lctrl.py stop

coverage erase
sh redis_init.sh stop
rm -f redis.pid dump.rdb redis.log
sh redis_init.sh start
python insert_test_stream.py

rm -f nohup.out *.log
coverage run --omit='/usr*' -p  --branch ../lctrl.py start
sleep 3

for f in test_*.py
do
	echo "processing $f ..."
	python $f
done

sleep 3
coverage run -p  --branch ../lctrl.py stop
sleep 3

sh redis_init.sh stop

coverage combine
coverage report --omit='/usr*' -m
coverage html
