#!/usr/local/bin/python2.7
#import gevent
import os
import string
import time
#from gevent import socket
#from gevent import monkey;
#monkey.patch_socket()
import sys
import threading
import httplib
import urllib

TIMEOUT = 100
data = {}
data["ok"] = 0
data["failed"] = 0

def http_req(i):
#    print "the %d threading start" % i
    url = "/get_pi?mid=1"
    conn = httplib.HTTPConnection("10.10.69.195",18090)
    conn.request('GET',url)
    rsp =  conn.getresponse()
#    print "the %d rsp get" % i
    conn.close()
    if rsp.status == 302:
        data["ok"] += 1
    elif rsp.status != 302:
        data["failed"] += 1

def multi_run(process_num):
    jobs = [gevent.spawn(http_req,i) for i in range(process_num)]
    gevent.joinall(jobs,timeout =TIMEOUT)

def multi_run_threading(process_num):
    t1 = [threading.Thread(target=http_req, args=(i,)) for i in range(process_num)]
    for item in t1:
        item.start()
if __name__ == '__main__':
    process_num = int(sys.argv[1])
    multi_run_threading(process_num)
    time.sleep(2)
    print data
