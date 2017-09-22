#include "udp_socket.h"
#include "util/util.h"

#include <iostream>

using namespace std;

namespace net {
    UDPSocket::UDPSocket() 
    {
    }

    UDPSocket::~UDPSocket()
    {
    }

    int UDPSocket::Bind(uint32_t local_ip, uint16_t local_port)
    {
        _local_ip = local_ip;
        _local_port = local_port;
        bzero(&_address, sizeof(_address));
        _address.sin_family = AF_INET;
        _address.sin_addr.s_addr = htonl(local_ip);
        _address.sin_port = htons(local_port);
        _socket = socket(AF_INET, SOCK_DGRAM, 0);

        fcntl(_socket, F_SETFD, FD_CLOEXEC);
        int ret = util_set_nonblock(_socket, TRUE);
        if (0 != ret) { return -1; }
        int opt = 1;
        ret = setsockopt(_socket, IPPROTO_IP, IP_PKTINFO, &opt, sizeof(opt));
        if (0 != ret) { return -1; }
        ret = bind(_socket, (struct sockaddr*)&_address, sizeof(_address));
        if (0 != ret) { return -1; }
        return 0;
    }

    int UDPSocket::get()
    {
        return _socket;
    }
} // namespace net
