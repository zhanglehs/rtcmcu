#ifndef _LCDN_NET_INETADDRESS_H_
#define _LCDN_NET_INETADDRESS_H_

#include <string>
#include <netinet/in.h>

namespace lcdn
{
namespace net
{

///
/// Wrapper of sockaddr_in.
///
class InetAddress
{
public:
    /// Constructs an endpoint with given port number.
    /// Mostly used in TcpServer listening.
    explicit InetAddress(uint16_t port);

    /// Constructs an endpoint with given ip and port.
    /// @c ip should be "1.2.3.4"
    InetAddress(const std::string& ip, uint16_t port);

    /// Constructs an endpoint with given struct @c sockaddr_in
    /// Mostly used when accepting new connections
    InetAddress(const struct sockaddr_in& addr)
        : _addr(addr)
    { }

    std::string to_host_port() const;

    // default copy/assignment are Okay

    const struct sockaddr_in& get_sockaddr_inet() const { return _addr; }
    void set_sockaddr_inet(const struct sockaddr_in& addr) { _addr = addr; }

private:
    struct sockaddr_in _addr;
};

} // namespace net
} // namespace lcdn

#endif  // _LCDN_NET_INETADDRESS_H_
