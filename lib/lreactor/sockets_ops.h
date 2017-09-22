#ifndef _LCDN_NET_SOCKETSOPS_H_
#define _LCDN_NET_SOCKETSOPS_H_

#include <arpa/inet.h>

namespace lcdn
{
namespace net
{
namespace sockets
{

// the inline assembler code makes type blur,
// so we disable warnings for a while.
#pragma GCC diagnostic ignored "-Wconversion"
inline uint32_t host_to_network32(uint32_t hostlong)
{
    return htonl(hostlong);
}

inline uint16_t host_to_network16(uint16_t hostshort)
{
    return htons(hostshort);
}

inline uint32_t network_to_host32(uint32_t netlong)
{
    return ntohl(netlong);
}

inline uint16_t network_to_host16(uint16_t netshort)
{
    return ntohs(netshort);
}
#pragma GCC diagnostic error "-Wconversion"

// Creates a non-blocking socket file descriptor
int create_nonblocking_or_die();

int  connect(int sockfd, const struct sockaddr_in& addr);
void bind_or_die(int sockfd, const struct sockaddr_in& addr);
void listen_or_die(int sockfd);
int  accept(int sockfd, struct sockaddr_in* addr);
void close(int sockfd);
void shutdown_write(int sockfd);

void to_host_port(char* buf, size_t size, const struct sockaddr_in& addr);
void from_host_port(const char* ip, uint16_t port, struct sockaddr_in* addr);

int get_socket_error(int sockfd);

struct sockaddr_in get_localaddr(int sockfd);
struct sockaddr_in get_peeraddr(int sockfd);
bool is_self_connect(int sockfd);

} // sockets
} // namespace net
} // namespace lcdn

#endif  // _LCDN_NET_SOCKETSOPS_H_
