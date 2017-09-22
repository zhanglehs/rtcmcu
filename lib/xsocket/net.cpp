#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "net.hpp"

int Net::InetAddressToSockaddr(unsigned int ia,  int port, struct sockaddr *him, int *len)
{
    struct sockaddr_in *him4 = (struct sockaddr_in*)him;

    memset((char *) him4, 0, sizeof(struct sockaddr_in));
    him4->sin_port = htons((short) port);
    him4->sin_addr.s_addr = (uint32_t) htonl(ia);
    him4->sin_family = AF_INET;
    *len = sizeof(struct sockaddr_in);

    return 0;
}

int Net::UNSocketAddressToSockaddr(string un_path, struct sockaddr *him, int *len)
{
    struct sockaddr_un *him4 = (struct sockaddr_un*)him;

    memset((char *) him4, 0, sizeof(struct sockaddr_un));

    strncpy(him4->sun_path,un_path.c_str(), sizeof(him4->sun_path));
    him4->sun_family = AF_UNIX;
    *len = sizeof(struct sockaddr_un);

    return 0;
}

int Net::SockaddrToInetAddress(struct sockaddr *him, unsigned int& ia,int& port)
{
    return 0;
}


InetSocketAddress* Net::checkAddress(SocketAddress* sa)
{
    /*
    if (sa == null)
    throw new IllegalArgumentException();
    if (!(sa instanceof InetSocketAddress))
    throw new UnsupportedAddressTypeException(); // ## needs arg
    */
    InetSocketAddress* isa = (InetSocketAddress*)sa;
    /*
    if (isa.isUnresolved())
        throw new UnresolvedAddressException(); // ## needs arg
    */
    return isa;
}

InetSocketAddress* Net::asInetSocketAddress(SocketAddress* sa)
{
    /*
    	if (!(sa instanceof InetSocketAddress))
    	    throw new UnsupportedAddressTypeException();
    */
    return (InetSocketAddress*)sa;
}

// -- Socket operations --

int Net::socket(bool stream,bool reuse)
{
    int fd;

    fd = ::socket(AF_INET, (stream ? SOCK_STREAM : SOCK_DGRAM), 0);

    if (fd < 0)
    {
        return -1;
    }
    if (reuse)
    {

        if (set_bool_option(fd, SOL_SOCKET, SO_REUSEADDR,true) < 0)
        {
            return -1;
        }
    }
    return fd;
}

int Net::serverSocket(bool stream)
{
    return socket(stream, true);
}

int Net::configure_blocking(int fd, bool blocking)
{
    int flags = fcntl(fd, F_GETFL);

    if ((blocking == false) && !(flags & O_NONBLOCK))
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    else if ((blocking == true) && (flags & O_NONBLOCK))
        return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    return 0;
}
int  Net::bind(int fd,unsigned int addr, int port)
{
    sockaddr sa;
    int sa_len = sizeof(sockaddr);
    int rv = 0;

    InetAddressToSockaddr(addr,port, (struct sockaddr *)&sa, &sa_len);
    rv =  ::bind(fd,(struct sockaddr *)&sa, sa_len);

    if (rv != 0) {
        return -1;
    }
    return 0;
}
int Net::listen(int fd, int backlog)
{
    if (::listen(fd, backlog) < 0)
        return -1;
    return 0;
}
int Net::connect(int fd,unsigned int remote, int remotePort,int trafficClass)
{
    sockaddr sa;
    int sa_len = sizeof(sockaddr);
    int rv;

    InetAddressToSockaddr(remote, remotePort, (struct sockaddr *) &sa, &sa_len);

    rv = ::connect(fd, (struct sockaddr *)&sa, sa_len);
    if (rv != 0)
    {
        if (errno == EINPROGRESS)
        {
            return -1;
        }
        else if (errno == EINTR)
        {
            return -1;
        }
        return -1;
    }
    return 0;
}

int Net::accept(int ssfd, int& newfd, InetSocketAddress& sa_in)
{
    struct sockaddr_in sa;
    unsigned int sa_len;
    sa_len = sizeof(struct sockaddr_in);
    /*
     * accept connection but ignore ECONNABORTED indicating that
     * a connection was eagerly accepted but was reset before
     * accept() was called.
     */
    for (;;)
    {
        newfd = ::accept(ssfd,  (sockaddr *)&sa, (socklen_t *)&sa_len);
        if (newfd >= 0) 
        {
            InetSocketAddress& remote_addr =  (InetSocketAddress&)sa_in;
            new(&remote_addr)InetSocketAddress(ntohl(sa.sin_addr.s_addr), ntohs(sa.sin_port));
            break;
        }
        if (errno != ECONNABORTED) {
            break;
        }
        /* ECONNABORTED => restart accept */
    }

    if (newfd < 0)
    {
        if (errno == EAGAIN)
            return -1;
        if (errno == EINTR)
            return -1;
        return -1;
    }

    return 0;
}

int Net::close(int fd)
{
    return ::close(fd);
}

int Net::shutdown(int fd, int how)
{
    return   ::shutdown(fd, how);
    //handleSocketError(errno);

}

int Net::localPort(int fd)
{
    sockaddr sa;
    unsigned int sa_len = sizeof(sockaddr);
    if (getsockname(fd, (struct sockaddr *)&sa, &sa_len) < 0)
    {
        //handleSocketError(errno);
        return -1;
    }
    sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(&sa);
    return ntohs(sin->sin_port);
}

int  Net::localInetAddress(int fd)
{
    sockaddr sa;
    unsigned int sa_len = sizeof(sockaddr);
    // int port;
    if (getsockname(fd, (struct sockaddr *)&sa, &sa_len) < 0) {
        //handleSocketError(errno);
        return -1;
    }

    sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(&sa);
    return ntohl(sin->sin_addr.s_addr);
}
InetSocketAddress Net::local_address(int fd)
{
    return  InetSocketAddress(localInetAddress(fd),localPort(fd));
}

int Net::localPortNumber(int fd)
{
    return localPort(fd);
}

int Net::get_int_option(int fd, int level, int opt)
{
    int result;
    struct linger linger;
    void *arg;
    unsigned int arglen;

    if (opt == SO_LINGER)
    {
        arg = (void *)&linger;
        arglen = sizeof(linger);
    } else {
        arg = (void *)&result;
        arglen = sizeof(result);
    }

    if (getsockopt(fd, level, opt, arg, &arglen) < 0)
    {
        return -1;
    }

    if (opt == SO_LINGER)
        return linger.l_onoff ? linger.l_linger : -1;
    else
        return result;
}

int Net::set_int_option(int fd, int level, int opt, int arg)
{
    int rv = 0;
    struct linger linger;
    void *parg;
    int arglen;

    if (opt == SO_LINGER)
    {
        parg = (void *)&linger;
        arglen = sizeof(linger);
        if (arg >= 0)
        {
            linger.l_onoff = 1;
            linger.l_linger = arg;
        } else {
            linger.l_onoff = 0;
            linger.l_linger = 0;
        }
    } else {
        parg = (void *)&arg;
        arglen = sizeof(arg);
    }

    rv = arg;

    if (setsockopt(fd, level, opt, parg, arglen) < 0)
    {
        rv = -1;
    }

    return rv;
}

in_addr_t Net::inet_addr(const char *cp)
{
    return ::inet_addr(cp);
}

bool Net::drain(int fd)
{
    char buf[128];
    int tn = 0;

    for (;;) {
        int n = ::read(fd, buf, sizeof(buf));
        tn += n;
        if ((n < 0) && (errno != EAGAIN))
            return false;
        if (n == (int)sizeof(buf))
            continue;
        return (tn > 0) ? true : false;
    }
}



int Net::unsocket(bool stream,bool reuse)
{
    int fd;

    fd = ::socket(AF_UNIX, (stream ? SOCK_STREAM : SOCK_DGRAM), 0);

    if (fd < 0)
    {
        return -1;
    }
    if (reuse)
    {
        if (set_bool_option(fd, SOL_SOCKET, SO_REUSEADDR,true) < 0)
        {
            return -1;
        }
    }
    return fd;
}

int Net::unserverSocket(bool stream)
{
    return socket(stream, true);
}


int  Net::unbind(int fd,string un_path)
{
    sockaddr sa;
    int sa_len = sizeof(sockaddr);
    int rv = 0;

    UNSocketAddressToSockaddr(un_path, (struct sockaddr *)&sa, &sa_len);
    rv =  ::bind(fd,(struct sockaddr *)&sa, sa_len);

    if (rv != 0) {
        return -1;
    }
    return 0;
}

int Net::unconnect(int fd, string un_path )
{
    sockaddr sa;
    int sa_len = sizeof(sockaddr);
    int rv;

    UNSocketAddressToSockaddr(un_path, (struct sockaddr *)&sa, &sa_len);

    rv = ::connect(fd, (struct sockaddr *)&sa, sa_len);
    if (rv != 0)
    {
        if (errno == EINPROGRESS)
        {
            return -1;
        }
        else if (errno == EINTR)
        {
            return -1;
        }
        return -1;
    }
    return 0;
}

int Net::unaccept(int ssfd, int& newfd, UNSocketAddress&)
{
    struct sockaddr sa;
    unsigned int sa_len;

    /*
     * accept connection but ignore ECONNABORTED indicating that
     * a connection was eagerly accepted but was reset before
     * accept() was called.
     */
    for (;;)
    {
        newfd = ::accept(ssfd, &sa, &sa_len);
        if (newfd >= 0) {
            break;
        }
        if (errno != ECONNABORTED) {
            break;
        }
        /* ECONNABORTED => restart accept */
    }

    if (newfd < 0)
    {
        if (errno == EAGAIN)
            return -1;
        if (errno == EINTR)
            return -1;
        return -1;
    }

    return 0;
}


