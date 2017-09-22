#include "ip_endpoint.h"
#include "sockets_ops.h"

#include <strings.h>
#include <netinet/in.h>


// INADDR_ANY use (type)value casting.
#pragma GCC diagnostic ignored "-Wold-style-cast"
static const in_addr_t kInaddrAny = INADDR_ANY;
#pragma GCC diagnostic error "-Wold-style-cast"

using namespace std;
using namespace lcdn;
using namespace lcdn::net;

InetAddress::InetAddress(uint16_t port)
{
    bzero(&_addr, sizeof _addr);
    _addr.sin_family = AF_INET;
    _addr.sin_addr.s_addr = sockets::host_to_network32(kInaddrAny);
    _addr.sin_port = sockets::host_to_network16(port);
}

InetAddress::InetAddress(const string& ip, uint16_t port)
{
    bzero(&_addr, sizeof _addr);
    sockets::from_host_port(ip.c_str(), port, &_addr);
}

string InetAddress::to_host_port() const
{
    char buf[32];
    sockets::to_host_port(buf, sizeof buf, _addr);
    return buf;
}

