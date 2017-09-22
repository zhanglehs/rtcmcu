import os
import socket
import httplib
import string
import urllib
import time
import threading


LISTEN_IP = '10.10.69.195'
LISTEN_PORT_GET = 10520
LISTEN_PORT_UP  = 10510
FILEPATH = "/home/interlive/1.flv"
TEST_TIME = 100

url_list_player = ["/v1/s?a=wuhahawuhahawuhahaluha&b=2",
                   "/v1/s?a=wuhahawuhahawuhahaluha&b=1",
                   "/v1/s?a=wuhahawuhahawuhahaluha&b=22",
                   "/v1/s?a=wuhahawuhahawuhahaluha&b=a",
                   "/v1/s?a=wuhahawuhahawuhahaluha&b=111111111111",
                   "/v2/s?a=wuhahawuhahawuhahaluha&b=2",
                   "/v2/a?",
                   "/",
                   "/a/b/c/d",
                   "/v1/get_stream?stream=22",
                   ]

url_list_uploader = ["/v2/get_state",
                     "/v1/get_state?programid=1&streamid=1&userid=1&key=1&localtimestamp=1",
                     "/v1/get_state?programid=a&streamid=a&userid=a&key=a&localtimestamp=a",
                     "/v1/getstate?programid=1&streamid=1&userid=1&key=1&localtimestamp=1",
                     "/v1/get_state?programid=1&streamid=1&userid=1&key=1",
                     "/v1/get_state?program_id=1&stream_id=1&user_id=1&key=1&localtimestamp=1",
                     "/v1/get_state?programid=1&streamid=111111111111&userid=1&key=1&localtimestamp=1",
                     "/v1/get_state?programid=-1&streamid=-1&userid=-1&key=-1&localtimestamp=-1",
                     ]

def sendmsg():
    print "sendmsg in"
    t_url = "a"
    for i in range(1111):
        t_url = t_url + "a"
    data = "GET /v1/s" + t_url + " HTTP/1.1 (CRLF) Accept:image/gif,image/x-xbitmap,image/jpeg,application/x-shockwave-flash,application/vnd.ms-excel,application/vnd.ms-powerpoint,application/msword,*/* (CRLF) Accept-language:zh-cn (CRLF) Accept-Encoding gzip,deflate (CRLF) User-Agent:Mozilla/4.0(compatible;MSIE6.0;Windows NT 5.0) (CRLF) Connection:Keep-Alive (CRLF) (CRLF)"
    sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    sock.connect((LISTEN_IP,LISTEN_PORT_GET))
    sock.send(data)
    sock.close()
    print "baozhaheader send ok"
    print "sendmsg out"

def muchconn():
    print "muchconn in"
    conn_list = []
    cnt = 0
    for i in range(1000):
        sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        try:
            sock.connect((LISTEN_IP,LISTEN_PORT_GET))
            sock.send("a")
            conn_list.append(sock)
            sock = 0
        except:
            cnt+=1
            print "connect failed %d times" % cnt
    print "muchconn send ok"
    print "muchconn out"

def sendhttp():
    print "sendhttp in"
    for item in url_list_player:
        conn = httplib.HTTPConnection(LISTEN_IP,LISTEN_PORT_GET)
        conn.request('GET',item)
        conn.close()

    for item in url_list_uploader:
        conn = httplib.HTTPConnection(LISTEN_IP,LISTEN_PORT_UP)
        conn.request('GET',item)
        conn.close()
    
    data =  urllib.urlencode({'b':'1'})
    headers = {"Content-type": "application/x-www-form-urlencoded",  
              "Accept": "text/plain"}
    conn = httplib.HTTPConnection(LISTEN_IP,LISTEN_PORT_GET)
    conn.request('POST', '/v1/s?a=wuhahawuhahawuhahaluha&b=2', body=data, headers=headers)
    conn.close()
    conn = httplib.HTTPConnection(LISTEN_IP,LISTEN_PORT_UP)
    conn.request('POST', '/v1/get_state?programid=1&b=1', body=data, headers=headers)
    conn.close()
    print "http send ok"
    print "sendhttp out"
    

def postdata():
    isend = 0
    tm = 0
    request_line = "POST /v1/post_stream?streamid=2&userid=34&key=3423 HTTP/1.1\r\nHOST:" + LISTEN_IP + ":" + str(LISTEN_PORT_UP) + "\r\n\r\n" 
    print len(request_line)
    fd = open(FILEPATH,'rb')
    sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    sock.connect((LISTEN_IP,LISTEN_PORT_UP))
    sock.send(request_line)
    while(isend != 1):
        tmp_data = fd.read(84500)
        if tmp_data == "" or tm == TEST_TIME:
            fd.close()
            isend = 1
        sock.send(tmp_data)
        time.sleep(1)
        tm += 1
        print tm
    sock.close()

def getdata():
    sendmsg()
    muchconn()
    sendhttp()

def multi_run():
    t1 = threading.Thread(target=postdata)
    t2 = threading.Thread(target=getdata)
    t1.start()
    t2.start()

if __name__ == '__main__':
    multi_run()
