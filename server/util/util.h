#ifndef UTIL_H_
#define UTIL_H_
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <sys/resource.h>
#include "common.h"

#if (defined __cplusplus && !defined _WIN32)
extern "C"
{
#endif
typedef enum
{
    HTTP_START = 0,
    HTTP_200 = 0,
    HTTP_400,
    HTTP_403,
    HTTP_404,
    HTTP_405,
    HTTP_408,
    HTTP_414,
    HTTP_421,
    HTTP_451,
    HTTP_500,
    HTTP_503,
    HTTP_509,
    HTTP_END,
} http_code;

#define util_min(x, y) ({							\
			typeof(x) min1__ = (x);					\
			typeof(y) min2__ = (y);					\
			(void) (&min1__ == &min2__);			\
			min1__ < min2__ ? min1__ : min2__; })

#define util_max(x, y) ({							\
			typeof(x) max1__ = (x);					\
			typeof(y) max2__ = (y);					\
			(void) (&max1__ == &max2__);			\
			max1__ > max2__ ? max1__ : max2__; })

static inline boolean
util_ipfilter_private(uint32_t ip_h)
{
    return (ip_h >= 0x0A000000 && ip_h <= 0x0AFFFFFF) ||
        (ip_h >= 0xAC100000 && ip_h <= 0xAC1FFFFF) ||
        (ip_h >= 0xC0A80000 && ip_h <= 0xC0A8FFFF);
}

static inline boolean
util_ipfilter_public(uint32_t ip_h)
{
    if((ip_h >= 0x01000000 && ip_h < 0x0A000000)
       || (ip_h > 0x0AFFFFFF && ip_h <= 0x7EFFFFFF))
        return TRUE;
    if((ip_h >= 0x80000000 && ip_h < 0xA9FE0000)
       || (ip_h > 0xA9FEFFFF && ip_h < 0xAC100000) || (ip_h > 0xAC1FFFFF
                                                       && ip_h <= 0xBFFFFFFF))
        return TRUE;
    if((ip_h >= 0xC0000000 && ip_h < 0xC0A80000)
       || (ip_h > 0xC0A8FFFF && ip_h <= 0xDFFFFFFF))
        return TRUE;
    return FALSE;
}

uint64_t util_get_curr_tick();
void util_set_cloexec(int fd, boolean is_set);
int util_memstr(const char *s, const char *find, size_t srclen,
                size_t findlen);
int util_set_nonblock(int fd, boolean is_set);
int util_daemonize(boolean is_nochdir, boolean is_noclose);

char *util_query_str_get(const char *src, size_t src_len, const char *key,
                         char *value_buf, size_t buf_len);

char *util_path_str_get(const char *src, size_t src_len, int idx,
                        char *value_buf, size_t buf_len);

int util_http_parse_req_line(const char *src, size_t src_len,
                             char *method, size_t method_len,
                             char *path, size_t path_len,
                             char *query_str, size_t query_str_len);

int util_ipstr2ip(const char *ip_or_domain, unsigned int *result);
int util_str2ip(const char *ip_or_domain, unsigned int *result);
char* util_ip2str_r(unsigned int host_order_ip, char *str, size_t sz);
const char *util_ip2str(unsigned int host_order_ip);
char* util_ip2str_no_r(unsigned int network_order_ip, char *str, size_t sz);
const char *util_ip2str_no(unsigned int network_order_ip);

int util_get_host_ip(const char *ip_or_domain, unsigned int *result);
int util_interface2ip(const char *interface, char *ip, size_t max_l);
int util_interface2ip_h(const char *interface, uint32_t * ip);
int util_get_interfaces(char **ifs, int max_cnt, int max_len, int *cnt);
int util_get_ips(char **ips, int max_cnt, int max_len, int *cnt,
                 boolean(*filter) (uint32_t ip));
int util_get_ip(uint32_t *ips, int max_cnt, int *cnt,
                 boolean(*filter) (uint32_t ip));
int util_create_pipe(int pipe_fd[2]);
int util_create_listen_fd(const char *ip, unsigned short port, int backlog);
int util_create_UDP_fd(unsigned int ip, unsigned short port,
                       unsigned int buf_sz);
int util_create_passive_fd(unsigned short port);
int util_fire_passive_fd(int fd, unsigned short port);
int util_set_limit(int resource, rlim_t cur, rlim_t max);
int util_unmask_cpu0();
boolean util_check_hex(const char *ptr, size_t len);
boolean util_check_digit(const char *ptr, size_t len);

/* networt to host seq of long long */
unsigned long long util_ntohll(unsigned long long val);

/* host to networt seq of long long */
unsigned long long util_htonll(unsigned long long val);
int util_gen_token(time_t time_val, uint32_t streamid,
                   const char *private_key, size_t key_len,
                   const char *para1, size_t para1_len,
                   const char *para2, size_t para2_len,
                   char *token, size_t token_len);

boolean util_check_token(time_t curr_time, uint32_t streamid,
                         const char *private_key, size_t ken_len,
                         const char *para1, size_t para1_len,
                         const char *para2, size_t para2_len,
                         unsigned int token_timeout, const char *token);

int util_hex_dump(const void *ptr, size_t len, char *buf, size_t buf_len);
void util_http_rsp_error_code(int fd, http_code code);
const char *util_http_code2numstr(http_code code);
const char *util_http_code2str(http_code code);
int util_create_pid_file(const char *path, const char *process_name);
#ifndef _WIN32
void itoa(int value, char *str);
#endif
#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
