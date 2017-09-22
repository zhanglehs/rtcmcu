import test_req_addr
import helper
import sys

sys.path.append('../')
import lutil



def check_server(server_ip, server_port):
    ret_set = set()

    for sid in range(1, 16):

        sock = helper.create_conn_impl(server_ip, server_port)
        (result, ip, port, token) = test_req_addr.get_req_addr_v2(sock, sid, '', 0, '')
        assert(0 == result)
        sock.close()

        sock = helper.create_conn_impl(server_ip, server_port)
        (result, new_ip, new_port, new_token) = test_req_addr.get_req_addr_v2(sock, sid, lutil.ip2str(ip), port, token)
        assert(0 == result)
        assert(ip == new_ip)
        assert(port == new_port)
        sock.close()

        addr = (lutil.ip2str(ip), port)
        if addr not in ret_set:
            ret_set.add(addr)



    print ret_set


server_list = [
    ("58.211.15.209" ,443),
    ("58.211.15.129" ,443),
    ("183.61.116.28" ,443),
    ("183.61.116.29" ,443),
    ("123.126.98.98" ,443),
    ("123.126.98.109",443),
    ("123.234.2.32"  ,443),
     ("123.234.2.31"  ,443),
]
#check_server(sys.argv[1], int(sys.argv[2]))

for s in server_list:
    print s[0],s[1]
    check_server(s[0], s[1])
