#include "socket.h"

#include "utils/logging.h"
#include "ip_endpoint.h"
#include "sockets_ops.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <strings.h>

using namespace lcdn;
using namespace lcdn::base;
using namespace lcdn::net;

Socket::~Socket()
{
    sockets::close(_sockfd);
}

void Socket::bind_address(const InetAddress& addr)
{
    sockets::bind_or_die(_sockfd, addr.get_sockaddr_inet());
}

void Socket::listen()
{
    sockets::listen_or_die(_sockfd);
}

int Socket::accept(InetAddress* peeraddr)
{
    struct sockaddr_in addr;
    bzero(&addr, sizeof addr);
    int connfd = sockets::accept(_sockfd, &addr);
    if (connfd >= 0)
    {
        peeraddr->set_sockaddr_inet(addr);
    }
    return connfd;
}

void Socket::shutdown_write()
{
    sockets::shutdown_write(_sockfd);
}

void Socket::set_tcp_nodelay(bool on)
{
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(_sockfd, IPPROTO_TCP, TCP_NODELAY,
                           &optval, sizeof optval);
    if (ret < 0)
    {
        LOG_ERROR("sockets::set_tcp_nodelay");
    }
}

void Socket::set_reuse_addr(bool on)
{
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR,
                           &optval, sizeof optval);
    if (ret < 0)
    {
        LOG_ERROR("sockets::set_reuse_addr");
    }
}

