/*
 *
 */

#include "net_utils.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <net/if.h>

union ip_t
{
    unsigned int ipi;
    struct ip_byte
    {
        unsigned char byte[4];
    } ipb;
};

int net_util_set_nonblock(int fd, int is_set)
{
    int flags = 1;  

//    struct linger ling = { 0, 0 };

    if(fd > 0) {
        /*
           setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (void *) &flags,
           sizeof (flags));
           setsockopt (fd, SOL_SOCKET, SO_KEEPALIVE, (void *) &flags,
           sizeof (flags));
           setsockopt (fd, SOL_SOCKET, SO_LINGER, (void *) &ling, sizeof (ling));
         */
        flags = fcntl(fd, F_GETFL);
        if(-1 == flags)
            return -1;
        if(is_set)
            flags |= O_NONBLOCK; 
        else
            flags &= ~O_NONBLOCK;
        if(0 != fcntl(fd, F_SETFL, flags))
            return -2;
    }
    return 0;
}

unsigned long long
net_util_ntohll(unsigned long long val)
{
    if(__BYTE_ORDER == __LITTLE_ENDIAN) {
        return (((unsigned long long) htonl((int) ((val << 32) >> 32))) <<
                32) | (unsigned int) htonl((int) (val >> 32));
    }
    else if(__BYTE_ORDER == __BIG_ENDIAN) {
        return val;
    }
}

unsigned long long
net_util_htonll(unsigned long long val)
{
    if(__BYTE_ORDER == __LITTLE_ENDIAN) {
        return (((unsigned long long) htonl((int) ((val << 32) >> 32))) <<
                32) | (unsigned int) htonl((int) (val >> 32));
    }
    else if(__BYTE_ORDER == __BIG_ENDIAN) {
        return val;
    }
}


int
net_util_create_listen_fd(const char *ip, unsigned short port, int backlog)
{
    int ret;
    int fd = -1;
    int val;
    struct linger lg;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == fd) {
        ret = -1;
        goto failed;
    }

    addr.sin_family = AF_INET;
    if(0 == inet_aton(ip, &(addr.sin_addr))) {
        ret = -2;
        goto failed;
    }
    addr.sin_port = htons(port);

    ret = fcntl(fd, F_SETFD, FD_CLOEXEC);
    if(-1 == ret) {
        ret = -3;
        goto failed;
    }

    ret = fcntl(fd, F_GETFL);
    if(-1 == ret) {
        ret = -4;
        goto failed;
    }

    ret = fcntl(fd, F_SETFL, ret | O_NONBLOCK);
    if(0 != ret) {
        ret = -5;
        goto failed;
    }

    val = 1;
    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if(0 != ret) {
        ret = -6;
        goto failed;
    }

    lg.l_onoff = 0;
    lg.l_linger = 0;
    ret = setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));

    ret = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
    if(0 != ret) {
        ret = -7;
        goto failed;
    }

    ret = listen(fd, backlog);
    if(0 != ret) {
        ret = -8;
        goto failed;
    }

    return fd;
  failed:
    if(-1 != fd)
        close(fd);
    return ret;
}

int
util_create_UDP_fd(unsigned int ip, unsigned short port, unsigned int buf_sz)
{
    int ret;
    int val;
    struct sockaddr_in addr;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if(-1 == fd)
        return -1;

    ret = fcntl(fd, F_SETFD, FD_CLOEXEC);
    if(-1 == ret)
        goto failed;

    ret = fcntl(fd, F_GETFL);
    if(-1 == ret)
        goto failed;

    ret = fcntl(fd, F_SETFL, ret | O_NONBLOCK);
    if(0 != ret)
        goto failed;

    val = buf_sz;
    ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
    if(0 != ret)
    {
    }
    ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
    if(0 != ret)
    {
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(ip);
    addr.sin_port = htons(port);
    ret = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
    if(0 != ret)
        goto failed;

    return fd;

  failed:
    if(-1 != fd)
        close(fd);
    return -1;
}

int
net_util_interface2ip_h(const char *interface, uint32_t * ip_h)
{
    int ret = inet_addr(interface);

    if((int)INADDR_NONE != ret) {
        *ip_h = ntohl(ret);
        return 0;
    }
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct ifreq ifr;

    if(-1 == fd)
        return -1;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, sizeof(ifr.ifr_name) - 1);
    ret = ioctl(fd, SIOCGIFADDR, &ifr);
    if(0 == ret) {
        struct sockaddr_in *saddr = (struct sockaddr_in *) &(ifr.ifr_addr);

        *ip_h = ntohl(saddr->sin_addr.s_addr);
    }
    else {
    }
    close(fd);
    fd = -1;

    return ret;
}

int
net_util_interface2ip(const char *interface, char *ip, size_t max_l)
{
    assert(max_l >= 16);
    if(max_l < INET_ADDRSTRLEN)
        return -1;
    uint32_t ip_h = 0;
    int ret = net_util_interface2ip_h(interface, &ip_h);

    if(0 != ret)
        return ret;

    net_util_ip2str_r(ip_h, ip, max_l);

    return ret;
}

const char *
net_util_ip2str(unsigned int host_order_ip)
{
    static char buf[128];
    memset(buf, 0, sizeof(buf));

    net_util_ip2str_r(host_order_ip, buf, sizeof(buf));
    return buf;
}

int
net_util_str2ip(const char *ip_or_domain, unsigned int *result)
{
    return net_util_get_host_ip(ip_or_domain, result);
}

char*
net_util_ip2str_no_r(unsigned int network_order_ip, char *str, size_t sz)
{
    assert(sz >= 16);
    union ip_t my_ip;

    my_ip.ipi = network_order_ip;

    memset(str, 0, sz);
    snprintf(str, sz, "%u.%u.%u.%u",
             (unsigned int) my_ip.ipb.byte[0],
             (unsigned int) my_ip.ipb.byte[1],
             (unsigned int) my_ip.ipb.byte[2],
             (unsigned int) my_ip.ipb.byte[3]);
    return str;
}
char*
net_util_ip2str_r(unsigned int host_order_ip, char *str, size_t sz)
{
    assert(sz >= 16);
    union ip_t my_ip;

    my_ip.ipi = host_order_ip;

    memset(str, 0, sz);
    snprintf(str, sz, "%u.%u.%u.%u",
             (unsigned int) my_ip.ipb.byte[3],
             (unsigned int) my_ip.ipb.byte[2],
             (unsigned int) my_ip.ipb.byte[1],
             (unsigned int) my_ip.ipb.byte[0]);
    return str;
}

const char *
net_util_ip2str_no(unsigned int network_order_ip)
{
    static char buf[128];

    net_util_ip2str_no_r(network_order_ip, buf, sizeof(buf));
    return buf;
}

int
net_util_get_host_ip(const char *ip_or_domain, unsigned int *result)
{
    if(NULL == result)
        return -1;

    unsigned int ret;

    ret = inet_addr(ip_or_domain);
    if(INADDR_NONE != ret) {
        *result = ntohl(ret);
        return 0;
    }

    struct hostent *ent = gethostbyname(ip_or_domain);

    if(NULL == ent) {
        return h_errno;
    }
    *result = ntohl(*(unsigned int *) (ent->h_addr));
    return 0;
}

int
net_util_get_interfaces(char **ifs, int max_cnt, int max_len, int *cnt)
{
    int rcnt = 0;
    char (*ifsr)[max_len] = (char (*)[max_len]) ifs;
    struct if_nameindex *if_ni, *i;

    if_ni = if_nameindex();
    if(if_ni == NULL) {
        return errno;
    }

    for(i = if_ni, rcnt = 0;
        !(i->if_index == 0 && i->if_name == NULL) && rcnt < max_cnt; i++) {
        strncpy(ifsr[rcnt], i->if_name, max_len);
        rcnt++;
    }

    if_freenameindex(if_ni);
    *cnt = rcnt;
    return 0;
}

int
net_util_get_ips(char **ips, int max_cnt, int max_len, int *cnt,
             bool (*filter) (uint32_t ip))
{
    assert(max_len >= 16);
    char (*ipsr)[max_len] = (char (*)[max_len]) ips;
    struct ifaddrs *ifaddr, *ifa;
    int rcnt = 0;
    uint32_t ip_h = 0;

    if(getifaddrs(&ifaddr) == -1) {
        return errno;
    }

    /* Walk through linked list, maintaining head pointer so we
     *               can free list later */

    for(ifa = ifaddr, rcnt = 0; ifa != NULL && rcnt < max_cnt;
        ifa = ifa->ifa_next) {
        if(ifa->ifa_addr == NULL || AF_INET != ifa->ifa_addr->sa_family)
            continue;

        ip_h = ntohl(((struct sockaddr_in *) ifa->ifa_addr)->sin_addr.s_addr);
        if(filter(ip_h)) {
            net_util_ip2str_r(ip_h, ipsr[rcnt], max_len);
            rcnt++;
        }
    }

    freeifaddrs(ifaddr);
    *cnt = rcnt;
    return 0;
}

bool 
net_util_ipfilter_private(uint32_t ip_h)
{
    return (ip_h >= 0x0A000000 && ip_h <= 0x0AFFFFFF) ||
        (ip_h >= 0xAC100000 && ip_h <= 0xAC1FFFFF) ||
        (ip_h >= 0xC0A80000 && ip_h <= 0xC0A8FFFF);
}

bool 
net_util_ipfilter_public(uint32_t ip_h)
{
    if((ip_h >= 0x01000000 && ip_h < 0x0A000000)
       || (ip_h > 0x0AFFFFFF && ip_h <= 0x7EFFFFFF))
        return true;
    if((ip_h >= 0x80000000 && ip_h < 0xA9FE0000)
       || (ip_h > 0xA9FEFFFF && ip_h < 0xAC100000) || (ip_h > 0xAC1FFFFF
                                                       && ip_h <= 0xBFFFFFFF))
        return true;
    if((ip_h >= 0xC0000000 && ip_h < 0xC0A80000)
       || (ip_h > 0xC0A8FFFF && ip_h <= 0xDFFFFFFF))
        return true;
    return false;
}
