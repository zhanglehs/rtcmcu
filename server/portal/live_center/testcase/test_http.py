import socket
import addr
import time
import sys
import urllib
import json

sys.path.append('../')
import lhttp

def test_unsupport_method():
    (ret, _) = addr.call_api('HEAD', '/')
    assert(405 == ret)


def test_timeout():
    sock = addr.create_connection()
    sock.send("localhost\r\n")
    time.sleep(lhttp.MAX_TIMEOUT / 1000 + 3)
    try:
        sock.send("Ua:my\r\n")
        sock.send("Ua:my\r\n")
        assert(False)
    except socket.error:
        pass

def test_invalid_first_line():
    sock = addr.create_connection()
    sock.send("localhost\r\n\r\n")
    line = sock.recv(4 * 1024)
    assert(len(line) > 0)
    assert(line.strip().startswith('HTTP/1.1 500 '))


def test_too_max_header():
    sock = addr.create_connection()
    max_header = "my:value\r\n" * 1024
    try:
        sock.send(max_header)
        rsp = sock.recv(1024)
        assert(0 == len(rsp))
    except socket.error:
        return
    assert(False)

def test_get_root():
    (ret, _) = addr.call_api("GET", "/")
    assert(ret == 200)

def test_close1():
    sock = addr.create_connection()
    sock.send("localhost\r\n")
    sock.close()

def test_close2():
    sock = addr.create_connection()
    sock.send("GET / HTTP/1.1\r\n")
    sock.close()



def test_get_all_server():
    addr.url_get_json('/get_all_server')

def test_get_all_stream():
    addr.url_get_json('/get_all_stream')


def test_get_server_stat():
    addr.url_get_json('/get_server_stat')

def test_get_forward_stat():
    addr.url_get_json('/get_forward_stat')

def test_get_receiver_stat():
    addr.url_get_json('/get_receiver_stat')

test_unsupport_method()
test_invalid_first_line()
test_too_max_header()
test_get_root()
test_close1()
test_close2()
test_timeout()
test_get_all_server()
test_get_all_stream()
test_get_server_stat()
test_get_forward_stat()
test_get_receiver_stat()
