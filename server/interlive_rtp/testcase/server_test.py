#!/usr/local/bin/python2.7
import gevent
import os
import string
from gevent import socket
from gevent import monkey;
monkey.patch_socket()
import sys
import threading
#import socket

IP = "10.10.69.195"
PORT = 10001
TIMEOUT = 10
data = {}

def creat_connect():
    sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    try:
        sock.connect((IP,PORT))
        print "creat success"
        return sock
    except:
        print "creat error"
        sock.close()
        return False

def get_stream(sock, i):
    sig = 1
    tmp_url = "/v1/s?a=wuhahawuhahawuhahaluha&b=1"
    req_line = "GET " + tmp_url + " HTTP/1.1\r\nHOST:" + IP +":" + str(PORT) + "\r\n\r\n"
    try:
        sock.send(req_line)
    except:
        return False

    while(sig != 0):
        buf = sock.recv(1024)
        if not buf:
            print "disconnect!"
            break;
        data[i] += 1
        gevent.sleep(0)

    sock.close()

def http_req(i):
    sock = creat_connect()
    get_stream(sock, i)


def multi_run_thread(process_num):
     t1 = [threading.Thread(target=http_req) for i in range(process_num)]
     for item in t1:
         item.start()
         print "thread start"

def multi_run(process_num):
    jobs = [gevent.spawn(http_req, i) for i in range(process_num)]
    gevent.joinall(jobs,timeout =TIMEOUT)

if __name__ == '__main__':
    process_num = int(sys.argv[1])
    print process_num
    for i in range(process_num):
        data[i] = 0
    multi_run(process_num)
#   multi_run_thread(process_num)
    for k,v in data.items():
        data[k] = v/process_num
    print data
