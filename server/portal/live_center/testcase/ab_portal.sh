#!/bin/sh
ab -c 10000 -n 100000 http://10.10.69.195:18090/get_pi?mid=1
