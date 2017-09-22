#!/bin/sh
sleep 3

coverage run -p  --branch ../lup_sche.py stop

coverage erase

rm -f nohup.out *.log
coverage run -p  --branch ../lup_sche.py start
sleep 3

for f in test_*.py
do
	echo "processing $f ..."
	python $f
done

coverage run  --branch ../lup_sche.py stop
sleep 3

coverage combine
coverage report -m
coverage html
