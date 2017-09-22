import sys
import struct
import socket
import select

sys.path.append('../')
import lproto


gaddr = "127.0.0.1"
gport = 19090
gtimeout = 4 * 1000

def recvn(sock, size):
    buf = ''
    while size > 0:
        tmp = sock.recv(size)
        if 0 == len(tmp):
            return ''
        buf += tmp
        size -= len(tmp)
    return buf

def recv_payload(sock):
    head = recvn(sock, struct.calcsize(lproto.comm_head_pat))
    if 0 == len(head):
        return (0, '')
    (magic, _, cmd, size) = struct.unpack(lproto.comm_head_pat, head)
    assert(0xff == magic)

    buf = recvn(sock, size - struct.calcsize(lproto.comm_head_pat))
    return (cmd, buf)

def create_conn_impl(my_ip, my_port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
    sock.connect((my_ip, my_port))
    return sock

def create_bin_conn():
    return create_conn_impl(gaddr, gport)

def check_close(sock, timeout = 1.0):
    (rl, _, _) = select.select([sock], [], [], timeout)
    if sock in rl:
        try:
            buf = sock.recv(1024)
            if 0 == len(buf):
                return True
        except socket.error:
            pass

    return False

