import sys
import helper
import time
import socket
import struct

sys.path.append('../')
import lproto


def check_timeout(sock):
    assert(helper.check_close(sock, helper.gtimeout))

def test_little_pkg():
    sock = helper.create_bin_conn()
    pkg = lproto.gen_pkg(1, '', pkg_size = struct.calcsize(lproto.comm_head_pat) / 2)
    sock.send(pkg)
    assert(helper.check_close(sock))

def test_max_pkg():
    sock = helper.create_bin_conn()
    pkg = lproto.gen_pkg(1, '', lproto.MAX_PKG_SIZE)
    sock.send(pkg)
    assert(helper.check_close(sock))

def test_half_pkg():
    sock = helper.create_bin_conn()
    payload = '0' * 128
    pkg = lproto.gen_pkg(1, payload)
    sock.send(pkg[0:len(pkg) / 2])
    assert(helper.check_close(sock))

def test_unknown_pkg():
    sock = helper.create_bin_conn()
    pkg = lproto.gen_pkg(0xFFFF, '', 0)
    sock.send(pkg)
    check_timeout(sock)

def test_timeout():
    sock = helper.create_bin_conn()
    check_timeout(sock)

    sock = helper.create_bin_conn()
    sock.send('a')
    time.sleep(1)
    sock.send('b')
    check_timeout(sock)

def test_conn_die():
    sock = helper.create_bin_conn()
    time.sleep(1)
    sock.shutdown(socket.SHUT_WR)
    try:
        buf = sock.recv(1024)
        assert(0 == len(buf))
    except socket.error:
        pass

def test_send_double_pkg():
    payload = struct.pack(lproto.s2s_dummy_pat, 0xAABBCCDD)
    pkg = lproto.gen_pkg(lproto.s2s_dummy_cmd, payload)

    pkg = pkg + pkg

    sock = helper.create_bin_conn()
    sock.send(pkg)
    (cmd, payload) = helper.recv_payload(sock)
    assert(lproto.s2s_dummy_cmd == cmd)
    assert(len(payload) == struct.calcsize(lproto.s2s_dummy_pat))
    (cmd, payload) = helper.recv_payload(sock)
    assert(lproto.s2s_dummy_cmd == cmd)
    assert(len(payload) == struct.calcsize(lproto.s2s_dummy_pat))
    check_timeout(sock)

if __name__ == "__main__":
    test_timeout()
    test_max_pkg()
    test_conn_die()
    test_unknown_pkg()
    test_little_pkg()
    test_send_double_pkg()
    test_half_pkg()
