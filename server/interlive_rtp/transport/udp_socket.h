#ifndef NET_UDP_UDP_SOCKET_H_
#define NET_UDP_UDP_SOCKET_H_

#include "utils/buffer.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

namespace net {

    class UDPSocket {
    public:
        UDPSocket();
        virtual ~UDPSocket();
        int Bind(uint32_t local_ip, uint16_t local_port);
        int get();
    private:
        uint32_t _local_ip;
        uint16_t _local_port;
        int _socket;
        struct sockaddr_in _address;
    };

}  // namespace net

#endif // NET_UDP_UDP_SOCKET_H_
