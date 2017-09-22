
#ifndef _LCDN_NET_SOCKET_H_
#define _LCDN_NET_SOCKET_H_
#include <iostream>
namespace lcdn
{
namespace net
{

class InetAddress;

///
/// Wrapper of socket file descriptor.
///
/// It closes the sockfd when desctructs.
class Socket
{
public:
    explicit Socket(int sockfd)
        : _sockfd(sockfd)
    { }

    ~Socket();

    int fd() const { return _sockfd; }

    void bind_address(const InetAddress& localaddr);
    void listen();

    /// On success, returns a non-negative integer that is
    /// a descriptor for the accepted socket, which has been
    /// set to non-blocking and close-on-exec. *peeraddr is assigned.
    /// On error, -1 is returned, and *peeraddr is untouched.
    int accept(InetAddress* peeraddr);

    void shutdown_write();

    ///
    /// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
    ///
    void set_tcp_nodelay(bool on);

    ///
    /// Enable/disable SO_REUSEADDR
    ///
    void set_reuse_addr(bool on);

private:
    const int _sockfd;
};

} // namespace net
} // namespace lcdn

#endif  // _LCDN_NET_SOCKET_H_
