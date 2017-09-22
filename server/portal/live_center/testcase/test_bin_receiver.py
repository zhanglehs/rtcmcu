import sys
import addr
import time
import socket
import struct


sys.path.append('../')
import lbin
import test_bin


def gen_rcv_keepalive_pkg_impl(ip, port, stream_list):
    stream_st = ''
    for sl in stream_list:
        stream_st += struct.pack(lbin.receiver_stream_status_pat,
                                sl['streamid'],
                                sl['forward_cnt'],
                                sl['keyframe_ts'],
                                sl['block_seq'])

    keepalive = struct.pack(lbin.r2p_keepalive_pat,
                            ip, port, 128 * 1024, 64 * 1024,
                            len(stream_list))
    keepalive += stream_st

    pkg_size = len(keepalive)
    pkg_size += struct.calcsize(lbin.comm_head_pat)
    pkg = struct.pack(lbin.comm_head_pat, 0xFF, 0x01,
                      lbin.r2p_keepalive_cmd, pkg_size)
    pkg += keepalive
    return pkg


def gen_rcv_keepalive_pkg(ip, port, stream_id_list):
    slist = []
    for stream_id in stream_id_list:
        sl = {}
        sl['streamid'] = stream_id
        sl['forward_cnt'] = 10
        sl['keyframe_ts'] = 100
        sl['block_seq'] = int(time.time())
        slist.append(sl)

    return gen_rcv_keepalive_pkg_impl(ip, port, slist)


def receiver_keepalive(sock, stream_id_list, ip_str, port):
    ip = struct.unpack('!I', socket.inet_aton(ip_str))[0]
    pkg = gen_rcv_keepalive_pkg(ip, port, stream_id_list)
    sock.send(pkg)

    (cmd, pkg) = test_bin.recv_payload(sock)
    assert(lbin.p2f_inf_stream_cmd == cmd)

    assert(len(pkg) > 0)
    stream_list = test_bin.parse_inf_stream(pkg)
    assert(len(stream_list) > 0)

    for sid in stream_id_list:
        if sid not in stream_list:
            assert(False)

def test_receiver_keepalive():
    sock = addr.create_bin_conn()
    a_id = addr.create_stream('91234', '1234', '81234')
    (cmd, pkg) = test_bin.recv_payload(sock)
    if lbin.p2f_close_stream_cmd == cmd:
        (cmd, pkg) = test_bin.recv_payload(sock)
        assert(lbin.p2f_start_stream_cmd == cmd)
    else:
        assert(lbin.p2f_start_stream_cmd == cmd)

    receiver_keepalive(sock, [a_id], '1.2.3.4', 9090)

    b_id = addr.create_stream('912347', '12347', '812347')
    (cmd, pkg) = test_bin.recv_payload(sock)
    if lbin.p2f_close_stream_cmd == cmd:
        (cmd, pkg) = test_bin.recv_payload(sock)
        assert(lbin.p2f_start_stream_cmd == cmd)
    else:
        assert(lbin.p2f_start_stream_cmd == cmd)

    receiver_keepalive(sock, [a_id, b_id], '1.2.3.4', 9090)


    # a_id is disappeared
    receiver_keepalive(sock, [b_id], '1.2.3.4', 9090)


    addr.destroy_stream('91234', '1234', '81234', str(a_id))
    (cmd, pkg) = test_bin.recv_payload(sock)
    assert(lbin.p2f_close_stream_cmd == cmd)

    receiver_keepalive(sock, [b_id], '1.2.3.4', 9090)

    #change receiver
    receiver_keepalive(sock, [b_id], '5.6.7.8', 9090)
    receiver_keepalive(sock, [], '1.2.3.4', 9090)

    addr.destroy_stream('912347', '12347', '812347', str(b_id))
    (cmd, pkg) = test_bin.recv_payload(sock)
    assert(lbin.p2f_close_stream_cmd == cmd)

    receiver_keepalive(sock, [], '1.2.3.4', 9090)

    sock.close()

if __name__ == "__main__":
    test_receiver_keepalive()
