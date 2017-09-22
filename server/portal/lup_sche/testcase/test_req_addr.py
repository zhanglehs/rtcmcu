import sys
import struct
import time
import helper

sys.path.append('../')
import lproto
import lutil
import lup_sche_conf

gtimeout = 4 * 1000


def get_req_addr_v2(sock, sid, old_ip, old_port, old_up_token = ''):
    curr_time = int(time.time())
    version = 1
    uploader_code = 2
    roomid = 3
    streamid = sid
    userid = 5
    sche_token = lutil.make_token(curr_time, streamid, str(userid), '', lup_sche_conf.up_sche_secret)
    up_token = ''
    if '' != old_up_token:
        up_token = old_up_token
    elif '' != old_ip:
        extra = '%s:%d' % (old_ip, old_port)
        up_token = lutil.make_token(curr_time, streamid, str(userid), extra,
                                    lup_sche_conf.upload_secret)
    ip = 0
    if '' != old_ip:
        ip = lutil.ipstr2ip(old_ip)
    port = old_port
    stream_type = 10
    pkg = struct.pack(lproto.u2us_req_addr_pat, version, uploader_code, roomid,
                      streamid, userid, sche_token, up_token, ip, port,
                      stream_type)
    pkg = lproto.gen_pkg(lproto.u2us_req_addr_cmd, pkg)
    sock.send(pkg)

    (cmd, pkg) = helper.recv_payload(sock)
    assert(cmd == lproto.us2u_rsp_addr_cmd)
    (ret_streamid, ip, port, result, token) = struct.unpack(lproto.us2u_rsp_addr_pat, pkg)

    assert(streamid == ret_streamid)
    if 0 == result:
        extra = '%s:%d' % (lutil.ip2str(ip), port)
        assert(lutil.check_token(curr_time, sid, str(userid), extra,
                                 lup_sche_conf.upload_secret, 1000, token))
    return (result, ip, port, token)

def get_req_addr(sock, sid= 100, def_token = '98765'):
    version = 1
    uploader_code = 2
    roomid = 3
    streamid = sid
    userid = 5
    sche_token = def_token
    up_token = def_token
    ip = 0
    port = 0
    stream_type = 10
    pkg = struct.pack(lproto.u2us_req_addr_pat, version, uploader_code, roomid,
                      streamid, userid, sche_token, up_token, ip, port,
                      stream_type)
    pkg = lproto.gen_pkg(lproto.u2us_req_addr_cmd, pkg)
    sock.send(pkg)

    (cmd, pkg) = helper.recv_payload(sock)
    assert(cmd == lproto.us2u_rsp_addr_cmd)
    (ret_streamid, ip, port, result, token) = struct.unpack(lproto.us2u_rsp_addr_pat, pkg)

    assert(streamid == ret_streamid)
    return (ip, port)


def test_req_ok():
    sock = helper.create_bin_conn()

    (result, ip, port, token) = get_req_addr_v2(sock, 123456, '', 0)
    assert(0 == result)
    assert(0 != ip)
    assert(0 != port)

    (result, ip, port, token) = get_req_addr_v2(sock, 123456, '127.0.0.1', 1000)
    assert(0 == result)
    assert(lutil.ip2str(ip) == '127.0.0.1')
    assert(port == 1000)

def test_req_fail():
    sock = helper.create_bin_conn()
    (ip, port) = get_req_addr(sock, 123, '01234567890123456789012345678901')
    assert(0 == ip)
    assert(0 == port)
    assert(helper.check_close(sock, gtimeout))

    sock = helper.create_bin_conn()
    (result, ip, port, token) = get_req_addr_v2(sock, 456, '127.0.0.1', 2000,
                                         '01234567890123456789012345678901')
    assert(0 != result)
    assert(0 == ip)
    assert(0 == port)
    assert(helper.check_close(sock, gtimeout))


def test_req_addr():
    sock = helper.create_bin_conn()
    (ip, port) = get_req_addr(sock)

    assert(0 != ip)
    assert(-1 != ip)
    assert(0 != port)

    assert(helper.check_close(sock, gtimeout))
    sock.close()

def test_timeout():
    sock = helper.create_bin_conn()
    assert(helper.check_close(sock, gtimeout))
    sock.close()

def test_req_diff():
    sock1 = helper.create_bin_conn()
    (ip1, port1) = get_req_addr(sock1, 1374029627)

    assert(0 != ip1)
    assert(-1 != ip1)
    assert(0 != port1)

    sock2 =  helper.create_bin_conn()
    (ip2, port2) = get_req_addr(sock2, 1374029628)

    assert(0 != ip2)
    assert(-1 != ip2)
    assert(0 != port2)

    sock3 = helper.create_bin_conn()
    (ip3, port3) = get_req_addr(sock3, 1374029629)

    assert(0 != ip3)
    assert(-1 != ip3)
    assert(0 != port3)

    print (ip1, port1)
    print (ip2, port2)
    print (ip3, port3)
    assert((ip1, port1) != (ip2, port2) or (ip1, port1) != (ip3, port3))

    assert(helper.check_close(sock1, gtimeout))
    assert(helper.check_close(sock2, gtimeout))
    assert(helper.check_close(sock3, gtimeout))

if __name__ == "__main__":
    test_req_ok()
    test_req_fail()
    test_req_addr()
    test_timeout()
    test_req_diff()
