#!/bin/sh
set -e
/usr/sbin/tcpkill -9 host 10.10.161.62 &>/dev/null &
pid=$!
echo \"$pid found\"
sleep 1
[ -n "$pid" ] && kill -9 $pid
