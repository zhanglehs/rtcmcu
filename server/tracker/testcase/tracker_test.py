#!/usr/local/bin/python2.7

import os
import string
import sys
import gevent
import struct
from gevent import monkey;
monkey.patch_socket()
from gevent import socket


IP = "10.10.69.195"
PORT = 1841

HEADER_MAGIC = 0
HEADER_VERSION = 1

PROTO_HEADER_FMT = "!BBHI"
REGISTER_REQ_FMT = PROTO_HEADER_FMT + "H"
REGISTER_RSQ_FMT = PROTO_HEADER_FMT + "H"
ADDR_REQ_FMT = PROTO_HEADER_FMT + "I" #Which forward has the stream in the packege
ADDR_RSP_FMT = PROTO_HEADER_FMT + "IHH"
UPDATE_STREAM_REQ_FMT = PROTO_HEADER_FMT + "HII" #report del or add stream info on local forward
UPDATE_STREAM_RSP_FMT = PROTO_HEADER_FMT + "H" 

CMD_F2T_REGISTER_REQ            = 200
CMD_F2T_REGISTER_RSP            = 201
CMD_F2T_ADDR_REQ                = 202
CMD_F2T_ADDR_RSP                = 203
CMD_F2T_UPDATE_STREAM_REQ       = 204
CMD_F2T_UPDATE_RESULT_RSP       = 205
CMD_F2T_KEEP_ALIVE              = 206
CMD_F2T_KEEP_ALIVE_RSP          = 207


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

def register():
    msg_list = [5100]
    send_msg(REGISTER_REQ_FMT, REGISTER_RSQ_FMT, CMD_F2T_REGISTER_REQ, CMD_F2T_REGISTER_RSP, msg_list)

def update_stream():
    msg_list = [1,1,0]
    send_msg(UPDATE_STREAM_REQ_FMT, UPDATE_STREAM_RSP_FMT, CMD_F2T_UPDATE_STREAM_REQ, CMD_F2T_UPDATE_RESULT_RSP, msg_list)

def addr():
    msg_list = [1]
    send_msg(ADDR_REQ_FMT, ADDR_RSP_FMT, CMD_F2T_ADDR_REQ, CMD_F2T_ADDR_RSP, msg_list)

def send_msg(req_fmt, rsp_fmt, req_cmd, rsp_cmd, msg_list):
    req_size = struct.calcsize(req_fmt)
    rsp_size = struct.calcsize(rsp_fmt)
    req_pkg  = struct.pack(req_fmt, HEADER_MAGIC, HEADER_VERSION, req_cmd, req_size, *msg_list)
    sock = creat_connect()
    try:
        sock.send(req_pkg)
        print "%s send success!" % req_cmd
    except:
        print "%s send failed!" % rsp_cmd
        return False

    try:
        tmp_msg = sock.recv(1024)
        if len(tmp_msg) != 0:
            rsp_msg = struct.unpack(rsp_fmt, tmp_msg)
            print "%s rsp success!" % rsp_cmd
    except:
        print "%s rsp failed!" % rsp_cmd
        return False
    sock.close()
       

def test1():
    register()
    update_stream()
    addr()

if __name__ == '__main__':
    test1()
