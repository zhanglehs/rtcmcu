import sys
import addr
import time
import socket
import struct
import select

sys.path.append('../')
import lbin

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
    head = recvn(sock, struct.calcsize(lbin.comm_head_pat))
    if 0 == len(head):
        return (0, '')
    (magic, _, cmd, size) = struct.unpack(lbin.comm_head_pat, head)
    assert(0xff == magic)

    buf = recvn(sock, size - struct.calcsize(lbin.comm_head_pat))
    return (cmd, buf)

def test_max_pkg():
    sock = addr.create_bin_conn()
    try:
        pkg = lbin.gen_pkg(1, '', lbin.MAX_PKG_SIZE)
        sock.send(pkg)
        assert(check_close(sock))
    except socket.error:
        pass

def test_half_pkg():
    sock = addr.create_bin_conn()
    payload = '0' * 128
    pkg = lbin.gen_pkg(1, payload)
    sock.send(pkg[0:len(pkg) / 2])
    assert(check_close(sock, 30))

def check_close(sock, timeout = 1.0):
    (rl, _, _) = select.select([sock], [], [], timeout)
    for s in rl:
        if sock == s:
            try:
                buf = sock.recv(1024)
                if 0 == len(buf):
                    return True
            except socket.error:
                pass

    return False

def check_timeout(sock):
    assert(check_close(sock, lbin.MAX_TIMEOUT / 1000 + 3))

def test_unknown_pkg():
    sock = addr.create_bin_conn()
    pkg = lbin.gen_pkg(0xFFFF, '', 0)
    sock.send(pkg)
    check_timeout(sock)

def test_timeout():
    sock = addr.create_bin_conn()
    check_timeout(sock)

    sock = addr.create_bin_conn()
    sock.send('a')
    sock.send('b')
    check_timeout(sock)

def test_conn_die():
    sock = addr.create_bin_conn()
    time.sleep(1)
    sock.shutdown(socket.SHUT_WR)
    try:
        buf = sock.recv(1024)
        assert(0 == len(buf))
    except socket.error:
        pass

def gen_fwd_keepalive_pkg_impl(ip, port, stream_list):
    stream_st = ''
    for sl in stream_list:
        stream_st += struct.pack(lbin.forward_stream_status_pat,
                                sl['streamid'],
                                sl['player_cnt'],
                                sl['forward_cnt'],
                                sl['keyframe_ts'],
                                sl['block_seq'])

    keepalive = struct.pack(lbin.f2p_keepalive_pat,
                            ip, port, 128 * 1024,
                            len(stream_list))
    keepalive += stream_st

    pkg_size = len(keepalive)
    pkg_size += struct.calcsize(lbin.comm_head_pat)
    pkg = struct.pack(lbin.comm_head_pat, 0xFF, 0x01,
                      lbin.f2p_keepalive_cmd, pkg_size)
    pkg += keepalive
    return pkg

def gen_fwd_keepalive_pkg(ip, port, stream_id_list):
    stream_list = []
    for sid in stream_id_list:
        sl = {}
        sl['streamid'] = sid
        sl['player_cnt'] = 10
        sl['forward_cnt'] = 10
        sl['keyframe_ts'] = 100
        sl['block_seq'] = int(time.time())
        stream_list.append(sl)
    return gen_fwd_keepalive_pkg_impl(ip, port, stream_list)

def parse_start_stream(pkg):
    (sid, ) = struct.unpack(lbin.p2f_start_stream_pat, pkg)
    return sid

def parse_close_stream(pkg):
    (sid, ) = struct.unpack(lbin.p2f_close_stream_pat, pkg)
    return sid

def parse_inf_stream(pkg):
    tmp_pkg = pkg[0:struct.calcsize(lbin.p2f_inf_stream_pat)]
    cnt = struct.unpack(lbin.p2f_inf_stream_pat, tmp_pkg)[0]
    pkg = pkg[struct.calcsize(lbin.p2f_inf_stream_pat):]
    stream_list = []
    while cnt > 0:
        cnt -= 1
        tmp_pkg = pkg[0:struct.calcsize(lbin.p2f_inf_stream_val_pat)]
        (stream_id,) = struct.unpack(lbin.p2f_inf_stream_val_pat, tmp_pkg)
        pkg = pkg[struct.calcsize(lbin.p2f_inf_stream_val_pat):]
        stream_list.append(stream_id)
    return stream_list

def send_keepalive_pkg(sock, sid_list, ip_str, port):
    ip = struct.unpack('!I', socket.inet_aton(ip_str))[0]
    pkg = gen_fwd_keepalive_pkg(ip, port, sid_list)
    sock.send(pkg)

def check_stream_list(sock, stream_id_list):
    (cmd, pkg) = recv_payload(sock)

    if lbin.p2f_inf_stream_cmd == cmd:
        assert(len(pkg) > 0)
        stream_list = parse_inf_stream(pkg)
        assert(len(stream_id_list) <= len(stream_list))

        for sid in stream_id_list:
            assert(stream_list.count(sid) == 1)
    else:
        assert(False)


def test_forward_keepalive():
    stream_id = addr.create_stream('612345', '12345', '512345')

    sock = addr.create_bin_conn()
    send_keepalive_pkg(sock, [stream_id], '1.2.2.2', 9090)

    check_stream_list(sock, [stream_id])

    send_keepalive_pkg(sock, [1, stream_id], '1.2.2.2', 9090)
    send_keepalive_pkg(sock, [stream_id], '1.2.2.2', 9090)

    addr.destroy_stream('612345', '12345', '512345', str(stream_id))
    sock.close()

    sock = addr.create_bin_conn()
    send_keepalive_pkg(sock, [1, 2, 3, 4, 5], '1.2.2.2', 9090)
    check_stream_list(sock, [])
    sock.close()


def test_forward_start_stream():
    sock = addr.create_bin_conn()
    send_keepalive_pkg(sock, [], '1.2.3.3', 9090)

    (cmd, pkg) = recv_payload(sock)
    if lbin.p2f_inf_stream_cmd == cmd:
        assert(len(pkg) > 0)
        parse_inf_stream(pkg)
    else:
        assert(False)

    stream_id = addr.create_stream('612345', '12345', '512345')
    (cmd, pkg) = recv_payload(sock)
    assert(cmd == lbin.p2f_start_stream_cmd)
    assert(stream_id == parse_start_stream(pkg))

    send_keepalive_pkg(sock, [stream_id], '1.4.3.2', 9090)
    check_stream_list(sock, [stream_id])

    addr.destroy_stream('612345', '12345', '512345', str(stream_id))

    (cmd, pkg) = recv_payload(sock)
    assert(cmd == lbin.p2f_close_stream_cmd)
    assert(stream_id == parse_close_stream(pkg))

    sock.close()


if __name__ == "__main__":
    test_timeout()
    test_max_pkg()
    test_conn_die()
    test_unknown_pkg()
    test_forward_keepalive()
    test_forward_start_stream()
    test_half_pkg()
