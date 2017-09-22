import os
import struct
import socket

class SortServer(object):
    """description of class"""
    def __init__(self):
        pass

    def inet_atoi32(self, ip_str):
        return struct.unpack("!I",socket.inet_aton(ip_str))[0]

    #return: ip,port
    def inet_64addr(self, ip_str,port):
        ip64 = self.inet_atoi32(ip_str) << 32
        port64 = 0x000000000000FFFF & port
        return ip64 | port64

    #return: ip,port1,port2
    def inet_64addr_ex(self, ip_str, port1, port2):
        ip64 = self.inet_atoi32(ip_str) << 32
        port64 = ((0x000000000000FFFF & port1) << 16) | (0x000000000000FFFF & port2)
        return ip64 | port64

    def inet_ip32_to_str(self, ip32):
        return socket.inet_ntoa(struct.pack('I',socket.htonl(ip32)))

    #return: ip,port
    def inet_get_addr_peer(self, ip64):
        ip32 = ip64 >> 32
        port = 0x000000000000FFFF & ip64
        addr_peer = []
        ip32_str = self.inet_ip32_to_str(ip32)
        addr_peer.append(ip32_str)
        addr_peer.append(int(port))
        return addr_peer

    #return: ip,port1,port2
    def inet_get_addr_peer_ex(self, ip64):
        ip32 = ip64 >> 32
        port = 0x00000000FFFFFFFF & ip64
        port1 = (port >> 16)
        port2 = port & 0xFFFF
        addr_peer = []
        ip32_str = self.inet_ip32_to_str(ip32)
        addr_peer.append(ip32_str)
        addr_peer.append(int(port1))
        addr_peer.append(int(port2))
        return addr_peer

    #suported format: ip:port,ip:port
    def sort_addr_list(self, addr_list):
        ret = []
        tmp = []
        for i in addr_list:
            tmp.append(self.inet_64addr(i[0],int(i[1])))
        tmp.sort()

        for j in tmp:
            ret.append(self.inet_get_addr_peer(j))

        return ret

    #suported format: ip:port1:port2,ip:port1:port2
    def sort_addr_list_ex(self, addr_list):
        ret = []
        tmp = []
        for i in addr_list:
            tmp.append(self.inet_64addr_ex(i[0], int(i[1]), int(i[2])))
        tmp.sort()

        for j in tmp:
            ret.append(self.inet_get_addr_peer_ex(j))

        return ret

    #suported format: [{ip,port,asn,region},{ip,port,asn,region}]
    def sort_addr_list_v3(self, addr_list, ip_name = 'ip', port_name = 'port', port_name1 = None):
        ret = []
        tmp = []
        tmp_orgn = {} #[ip64] = list_row
        for i in addr_list:
             if None == port_name1:
                 ip64 = self.inet_64addr(i[ip_name],int(i[port_name]))
             else:
                 ip64 = self.inet_64addr_ex(i[ip_name],int(i[port_name]),int(i[port_name1]))
             tmp_orgn[ip64] = i
             tmp.append(ip64)
        tmp.sort()

        for j in tmp:   
            ret.append(tmp_orgn[j])
        return ret
