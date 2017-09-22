#ifndef _BASE_NET_NET_H_
#define _BASE_NET_NET_H_
#include <unistd.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "socketaddress.hpp"
class Net
{						// package-private
public:
    Net() { }
    // -- Miscellaneous utilities --
    static long long  current_time_millis();    
    static int InetAddressToSockaddr(unsigned int ia,  int port, struct sockaddr *him, int *len);
    static int UNSocketAddressToSockaddr(string un_path, struct sockaddr *him, int *len);
    static int SockaddrToInetAddress(struct sockaddr *him,unsigned int& ia,int& port);
    //static int handleSocketError(int errno);
    static InetSocketAddress* checkAddress(SocketAddress* sa) ;

    static InetSocketAddress* asInetSocketAddress(SocketAddress* sa);
    static int socket(bool stream,bool reuse = true);

    static int serverSocket(bool stream);
    static int configure_blocking(int fd, bool blocking);
    static int accept(int ssfd, int& newfd, InetSocketAddress&);
    static  int bind(int fd, unsigned int addr, int port);
    static  int listen(int fd, int backlog);
    static  int connect(int fd,unsigned int remote,int remotePort,int trafficClass);

    static  int close(int fd);
    static int shutdown(int fd, int how);

    // unix domain
    static  int unsocket(bool stream,bool reuse = true);
    static  int unserverSocket(bool stream);
    static  int  unbind(int fd,string un_path);
    static  int unconnect(int fd, string un_path );
    static  int unaccept(int ssfd, int& newfd, UNSocketAddress&);
    static  int localPort(int fd);
    static  int localInetAddress(int fd);

    static InetSocketAddress local_address(int fd);
    static in_addr_t inet_addr(const char *cp);
    static int localPortNumber(int fd) ;


    static int get_int_option(int fd, int level, int opt);
    static int set_int_option(int fd, int level, int opt, int arg);

    static bool get_bool_option(int fd, int level, int opt);
    static int set_bool_option(int fd, int level, int opt, bool arg);

    static bool drain(int fd);

};

inline
bool Net::get_bool_option(int fd, int level, int opt)
{
    return get_int_option(fd, level, opt) > 0;
}

inline
int Net::set_bool_option(int fd, int level, int opt, bool b)
{
    return set_int_option(fd, level, opt, b ? 1 : 0);
}

inline
long long  Net::current_time_millis()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    		
    return  (long long )tv.tv_sec * 1000 + tv.tv_usec/1000;
}
    
#endif

