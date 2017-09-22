#include "sockets_ops.h"
#include "utils/logging.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <strings.h>  
#include <sys/socket.h>
#include <unistd.h>

using namespace lcdn;
using namespace lcdn::base;
using namespace lcdn::net;

namespace
{

template<typename To, typename From>
inline To implicit_cast(From const &f) 
{
    return f;
}

typedef struct sockaddr SA;

const SA* sockaddr_cast(const struct sockaddr_in* addr)
{
    return static_cast<const SA*>(implicit_cast<const void*>(addr));
}

SA* sockaddr_cast(struct sockaddr_in* addr)
{
    return static_cast<SA*>(implicit_cast<void*>(addr));
}

void set_nonblock_and_close_on_exec(int sockfd)
{
    // non-block
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);
    if (ret < 0)
    {
        LOG_FATAL("sockets::set_nonblock");
    }

    // close-on-exec
    flags = ::fcntl(sockfd, F_GETFD, 0);
    flags |= FD_CLOEXEC;
    ret = ::fcntl(sockfd, F_SETFD, flags);
    if (ret < 0)
    {
        LOG_FATAL("sockets::set_close_on_exec");
    }
}

} // namespace

int sockets::create_nonblocking_or_die()
{
    // socket
    int sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
    {
        LOG_FATAL("sockets::create_nonblocking_or_die");
    }

    set_nonblock_and_close_on_exec(sockfd);
    return sockfd;
}

void sockets::bind_or_die(int sockfd, const struct sockaddr_in& addr)
{
    int ret = ::bind(sockfd, sockaddr_cast(&addr), sizeof addr);
    if (ret < 0)
    {
        LOG_FATAL("sockets::bind_or_die");
    }
}

void sockets::listen_or_die(int sockfd)
{
    int ret = ::listen(sockfd, SOMAXCONN);
    if (ret < 0)
    {
        LOG_FATAL("sockets::listen_or_die");
    }
}

int sockets::accept(int sockfd, struct sockaddr_in* addr)
{
    socklen_t addrlen = sizeof *addr;
    int connfd = ::accept(sockfd, sockaddr_cast(addr), &addrlen);
    set_nonblock_and_close_on_exec(connfd);

    if (connfd < 0)
    {
        int saved_errno = errno;
        LOG_ERROR("sockets::accept");
        switch (saved_errno)
        {
        case EAGAIN:
        case ECONNABORTED:
        case EINTR:
        case EPROTO: // ???
        case EPERM:
            // expected errors
            break;
        case EBADF:
        case EFAULT:
        case EINVAL:
        case EMFILE: // per-process lmit of open file desctiptor ???
        case ENFILE:
        case ENOBUFS:
        case ENOMEM:
        case ENOTSOCK:
        case EOPNOTSUPP:
            LOG_FATAL("unexpected error of accept");
            break;
        default:
            LOG_FATAL("unknown error of accept " << saved_errno);
            break;
        }
    }
    return connfd;
}

int sockets::connect(int sockfd, const struct sockaddr_in& addr)
{
    return ::connect(sockfd, sockaddr_cast(&addr), sizeof addr);
}

void sockets::close(int sockfd)
{
    if (::close(sockfd) < 0)
    {
        LOG_ERROR("sockets::close");
    }
}

void sockets::shutdown_write(int sockfd)
{
    if (::shutdown(sockfd, SHUT_WR) < 0)
    {
        LOG_ERROR("sockets::shutdown_write");
    }
}

void sockets::to_host_port(char* buf, size_t size, const struct sockaddr_in& addr)
{
    char host[INET_ADDRSTRLEN] = "INVALID";
    ::inet_ntop(AF_INET, &addr.sin_addr, host, sizeof host);
    uint16_t port = sockets::network_to_host16(addr.sin_port);
    snprintf(buf, size, "%s:%u", host, port);
}

void sockets::from_host_port(const char* ip, uint16_t port, struct sockaddr_in* addr)
{
    addr->sin_family = AF_INET;
    addr->sin_port = network_to_host16(port);
    if (inet_pton(AF_INET, ip, &addr->sin_addr) <= 0)
    {
        LOG_ERROR("sockets::from_host_port");
    }
}

int sockets::get_socket_error(int sockfd)
{
    int optval;
    socklen_t optlen = sizeof optval;

    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        return errno;
    }
    else
    {
        return optval;
    }
}

struct sockaddr_in sockets::get_localaddr(int sockfd)
{
    struct sockaddr_in localaddr;
    bzero(&localaddr, sizeof localaddr);
    socklen_t addrlen = sizeof(localaddr);
    if (::getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) < 0)
    {
        LOG_ERROR("sockets::get_localaddr");
    }
    return localaddr;
}

struct sockaddr_in sockets::get_peeraddr(int sockfd)
{
    struct sockaddr_in peeraddr;
    bzero(&peeraddr, sizeof peeraddr);
    socklen_t addrlen = sizeof(peeraddr);
    if (::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) < 0)
    {
        LOG_ERROR("sockets::get_peeraddr");
    }
    return peeraddr;
}

bool sockets::is_self_connect(int sockfd)
{
    struct sockaddr_in localaddr = get_localaddr(sockfd);
    struct sockaddr_in peeraddr = get_peeraddr(sockfd);
    return localaddr.sin_port == peeraddr.sin_port
        && localaddr.sin_addr.s_addr == peeraddr.sin_addr.s_addr;
}

