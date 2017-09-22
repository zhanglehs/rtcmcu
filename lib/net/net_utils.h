/*
 *
 *
 */

#ifndef __NET_UTILS_H_
#define __NET_UTILS_H_

#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

int net_util_set_nonblock(int fd, int is_set);

/* networt to host seq of long long */
unsigned long long net_util_ntohll(unsigned long long val);

/* host to networt seq of long long */
unsigned long long net_util_htonll(unsigned long long val);
int net_util_create_listen_fd(const char *ip, unsigned short port, int backlog);
int net_util_create_UDP_fd(unsigned int ip, unsigned short port,
                       unsigned int buf_sz);
int net_util_interface2ip_h(const char *interface, uint32_t * ip);
int net_util_interface2ip(const char *interface, char *ip, size_t max_l);
const char *net_util_ip2str(unsigned int host_order_ip);
int net_util_str2ip(const char *ip_or_domain, unsigned int *result);
char* net_util_ip2str_r(unsigned int host_order_ip, char *str, size_t sz);
char* net_util_ip2str_no_r(unsigned int network_order_ip, char *str, size_t sz);
const char *net_util_ip2str_no(unsigned int network_order_ip);
int net_util_get_host_ip(const char *ip_or_domain, unsigned int *result);
int net_util_get_interfaces(char **ifs, int max_cnt, int max_len, int *cnt);
int net_util_get_ips(char **ips, int max_cnt, int max_len, int *cnt,
        bool (*filter) (uint32_t ip));

bool net_util_ipfilter_private(uint32_t ip_h);

bool net_util_ipfilter_public(uint32_t ip_h);

#ifdef __cplusplus
}
#endif

#endif

