#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#include "util.h"
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sched.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include "common.h"
#include "log.h"
#include "md5.h"

#define UTIL_ERR(...) fprintf(stderr, __VA_ARGS__);\
	fprintf(stderr, "\n")

static const char *g_http_code[HTTP_END][2] = {
	{ "200", "OK" },
	{ "400", "Bad Request" },
	{ "403", "Forbidden" },
	{ "404", "Not Found" },
	{ "405", "Method Not Allowed" },
	{ "408", "Request Timeout" },
	{ "414", "Request-URI Too Long" },
	{ "421", "There are too many connections from your internet address" },
	{ "500", "Internal Server Error" },
	{ "503", "Service Unavailable" },
	{ "509", "Bandwidth Limit Exceeded" },
};

uint64_t
util_get_curr_tick()
{
	struct timeval tv = { 0, 0 };
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000l + tv.tv_usec / 1000l;
}

void
util_set_cloexec(int fd, boolean on)
{
	fcntl(fd, F_SETFD, on ? FD_CLOEXEC : 0);
}

int
util_memstr(const char *s, const char *find, size_t srclen, size_t findlen)
{
	char *bp, *sp;
	int len = 0, success = 0;

	if (findlen == 0 || srclen < findlen)
		return -1;
	for (len = 0; len <= (int)(srclen - findlen); len++) {
		if (s[len] == find[0]) {
			bp = (char *)s + len;
			sp = (char *)find;
			do {
				if (!*sp) {
					success = 1;
					break;
				}
			} while (*bp++ == *sp++);
			if (success)
				break;
		}
	}

	if (success)
		return len;
	else
		return -1;
}

int
util_set_nonblock(int fd, boolean on)
{
	int flags = 1;

	//    struct linger ling = { 0, 0 };

	if (fd > 0) {
		/*
		setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (void *) &flags,
		sizeof (flags));
		setsockopt (fd, SOL_SOCKET, SO_KEEPALIVE, (void *) &flags,
		sizeof (flags));
		setsockopt (fd, SOL_SOCKET, SO_LINGER, (void *) &ling, sizeof (ling));
		*/
		flags = fcntl(fd, F_GETFL);
		if (-1 == flags)
			return -1;
		if (on)
			flags |= O_NONBLOCK;
		else
			flags &= ~O_NONBLOCK;
		if (0 != fcntl(fd, F_SETFL, flags))
			return -2;
	}
	return 0;
}

int
util_daemonize(boolean nochdir, boolean noclose)
{
	int fd;

	switch (fork()) {
	case -1:
		return (-1);
	case 0:
		break;
	default:
		_exit(EXIT_SUCCESS);
	}

	if (setsid() == -1)
		return (-1);

	if (!nochdir) {
		if (chdir("/") != 0) {
			perror("chdir");
			return (-1);
		}
	}

	if (!noclose && (fd = open("/dev/null", O_RDWR, 0)) != -1) {
		if (dup2(fd, STDIN_FILENO) < 0) {
			perror("dup2 stdin");
			return (-1);
		}

		if (dup2(fd, STDOUT_FILENO) < 0) {
			perror("dup2 stdout");
			return (-1);
		}

		if (dup2(fd, STDERR_FILENO) < 0) {
			perror("dup2 stderr");
			return (-1);
		}

		if (fd > STDERR_FILENO) {
			if (close(fd) < 0) {
				perror("close");
				return (-1);
			}
		}
	}

	return (0);
}

/*
* programid=XXX&streamid=XXX&userid=XXX[&xxx=XXX]
*
*/
char *
util_query_str_get(const char *src, size_t src_len, const char *key,
char *value_buf, size_t buf_len)
{
	if (NULL == src || NULL == key || NULL == value_buf || 0 == src_len ||
		src_len <= strlen(key) + 1 || buf_len <= 1)
		return NULL;

	const size_t key_len = strlen(key);
	char key_expand[key_len + 2];

	memcpy(key_expand, key, key_len);
	*(key_expand + key_len) = '=';
	*(key_expand + key_len + 1) = '\0';
	size_t key_expand_len = key_len + 1;
	const char *end_ptr = src + src_len;

	char *m_start_ptr = (char *)memmem(src, src_len, key_expand, key_expand_len);

	if (NULL == m_start_ptr)
		return NULL;
	char *v_start_ptr = m_start_ptr + key_expand_len;

	if (v_start_ptr >= end_ptr || '&' == *v_start_ptr)
		return NULL;

	char *v_end_ptr = (char *)memmem(v_start_ptr, end_ptr - v_start_ptr, "&", 1);

	if (NULL == v_end_ptr)
		v_end_ptr = (char *)end_ptr;

	if (v_end_ptr - v_start_ptr >= (int)buf_len)
		return NULL;

	memcpy(value_buf, v_start_ptr, v_end_ptr - v_start_ptr);
	value_buf[v_end_ptr - v_start_ptr] = '\0';
	return value_buf;
}

/*
* /live/flv/v1/XXXXXXXXXXXXXXXXXXXXXXXX.flv
*
*/
char *
util_path_str_get(const char *src, size_t src_len, int idx,
char *value_buf, size_t buf_len)
{
    if (NULL == src || NULL == value_buf || idx <= 0 || 0 == src_len || buf_len <= 1)
        return NULL;

    src_len = strlen(src);
    const char* src_origin = src;

    int i = 0;
    char *value_buf_tmp = value_buf;
    while (src < src_origin + src_len)
    {
        if ('/' == *src)
        { 
            i++; 
            if (i == idx)
            {
                size_t buf_cpy_len = 0;
                src++;
                while ('/' != *src && '\0' != *src)
                {
                    *value_buf_tmp++ = *src++;
                    buf_cpy_len++;
                }
                *value_buf_tmp = '\0';
                buf_cpy_len++;
                if (buf_cpy_len > buf_len)
                    return NULL;
            }
        }
        src++;
    }

    return value_buf;
}

/*
* GET/POST PATH?QUERY_STR HTTP/1.X
*
* src_len not include "\r\n"
*
* return -1: error
* return 0: contain method, path
* return 1: contain method, path, query_str
*/

int
util_http_parse_req_line(const char *src, size_t src_len,
char *method, size_t method_len,
char *path, size_t path_len,
char *query_str, size_t query_str_len)
{
	if (NULL == src || NULL == method || NULL == path || NULL == query_str)
		return -1;

	char *sp0 = (char *)memmem(src, src_len, " ", 1);

	if (NULL == sp0 || sp0 <= src || sp0 - src >= (int)method_len) {
		return -1;
	}
	char *src_end = (char *)src + src_len;
	char *sp1 = (char *)memmem(sp0 + 1, src_end - sp0 - 1, " ", 1);

	if (NULL == sp1 || sp1 <= sp0 + 1) {
		return -1;
	}

	memcpy(method, src, sp0 - src);
	method[sp0 - src] = '\0';

	char *qu = (char *)memmem(sp0 + 1, sp1 - sp0 - 1, "?", 1);

	if (NULL == qu) {
		if (sp1 - sp0 - 1 >= (int32_t)path_len) {
			return -1;
		}
		memcpy(path, sp0 + 1, sp1 - sp0 - 1);
		path[sp1 - sp0 - 1] = '\0';
		return 0;
	}
	else {
        if (sp0 + 1 == qu || qu - sp0 - 1 >= (int32_t)path_len
            || sp1 - qu - 1 >= (int32_t)query_str_len || sp1 - qu - 1 <= 0) {
			return -1;
		}
		memcpy(path, sp0 + 1, qu - sp0 - 1);
		path[qu - sp0 - 1] = '\0';
		memcpy(query_str, qu + 1, sp1 - qu - 1);
		query_str[sp1 - qu - 1] = '\0';
		return 1;
	}
}

union ip_t
{
	unsigned int ipi;
	struct ip_byte
	{
		unsigned char byte[4];
	} ipb;
};

char*
util_ip2str_r(unsigned int host_order_ip, char *str, size_t sz)
{
	assert(sz >= 16);
	union ip_t my_ip;

	my_ip.ipi = host_order_ip;

	memset(str, 0, sz);
	snprintf(str, sz, "%u.%u.%u.%u",
		(unsigned int)my_ip.ipb.byte[3],
		(unsigned int)my_ip.ipb.byte[2],
		(unsigned int)my_ip.ipb.byte[1],
		(unsigned int)my_ip.ipb.byte[0]);
	return str;
}

const char *
util_ip2str(unsigned int host_order_ip)
{
	static char buf[128];
	memset(buf, 0, sizeof(buf));

	util_ip2str_r(host_order_ip, buf, sizeof(buf));
	return buf;
}

char*
util_ip2str_no_r(unsigned int network_order_ip, char *str, size_t sz)
{
	assert(sz >= 16);
	union ip_t my_ip;

	my_ip.ipi = network_order_ip;

	memset(str, 0, sz);
	snprintf(str, sz, "%u.%u.%u.%u",
		(unsigned int)my_ip.ipb.byte[0],
		(unsigned int)my_ip.ipb.byte[1],
		(unsigned int)my_ip.ipb.byte[2],
		(unsigned int)my_ip.ipb.byte[3]);
	return str;
}

const char *
util_ip2str_no(unsigned int network_order_ip)
{
	static char buf[128];

	util_ip2str_no_r(network_order_ip, buf, sizeof(buf));
	return buf;
}

int
util_str2ip(const char *ip_or_domain, unsigned int *result)
{
	return util_get_host_ip(ip_or_domain, result);
}

int util_ipstr2ip(const char *ip_or_domain, unsigned int *result)
{
    if (NULL == result)
        return -1;

    unsigned int ret;

    ret = inet_addr(ip_or_domain);
    if (INADDR_NONE != ret) {
        *result = ntohl(ret);
        return 0;
    }
    return -1;
}


int
util_get_host_ip(const char *ip_or_domain, unsigned int *result)
{
	if (NULL == result)
		return -1;

	unsigned int ret;

	ret = inet_addr(ip_or_domain);
	if (INADDR_NONE != ret) {
		*result = ntohl(ret);
		return 0;
	}

	struct hostent *ent = gethostbyname(ip_or_domain);

	if (NULL == ent) {
		return h_errno;
	}
	*result = ntohl(*(unsigned int *)(ent->h_addr));
	return 0;
}

int
util_interface2ip_h(const char *interface, uint32_t * ip_h)
{
	int ret = inet_addr(interface);

	if ((int)INADDR_NONE != ret) {
		*ip_h = ntohl(ret);
		return 0;
	}
	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct ifreq ifr;

	if (-1 == fd)
		return -1;
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, interface, sizeof(ifr.ifr_name) - 1);
	ret = ioctl(fd, SIOCGIFADDR, &ifr);
	if (0 == ret) {
		struct sockaddr_in *saddr = (struct sockaddr_in *) &(ifr.ifr_addr);

		*ip_h = ntohl(saddr->sin_addr.s_addr);
	}
	else {
		ERR("can't do ioctl.error:%s.", strerror(errno));
	}
	close(fd);
	fd = -1;

	return ret;
}

int
util_interface2ip(const char *interface, char *ip, size_t max_l)
{
	assert(max_l >= 16);
	if (max_l < INET_ADDRSTRLEN)
		return -1;
	uint32_t ip_h = 0;
	int ret = util_interface2ip_h(interface, &ip_h);

	if (0 != ret)
		return ret;

	util_ip2str_r(ip_h, ip, max_l);

	return ret;
}

int
util_get_interfaces(char **ifs, int max_cnt, int max_len, int *cnt)
{
	int rcnt = 0;
	char(*ifsr)[max_len] = (char(*)[max_len]) ifs;
	struct if_nameindex *if_ni, *i;

	if_ni = if_nameindex();
	if (if_ni == NULL) {
		return errno;
	}

	for (i = if_ni, rcnt = 0;
		!(i->if_index == 0 && i->if_name == NULL) && rcnt < max_cnt; i++) {
		strncpy(ifsr[rcnt], i->if_name, max_len);
		rcnt++;
	}

	if_freenameindex(if_ni);
	*cnt = rcnt;
	return 0;
}

int
util_get_ips(char **ips, int max_cnt, int max_len, int *cnt,
boolean(*filter) (uint32_t ip))
{
	assert(max_len >= 16);
	char(*ipsr)[max_len] = (char(*)[max_len]) ips;
	struct ifaddrs *ifaddr, *ifa;
	int rcnt = 0;
	uint32_t ip_h = 0;

	if (getifaddrs(&ifaddr) == -1) {
		return errno;
	}

	/* Walk through linked list, maintaining head pointer so we
	*               can free list later */

	for (ifa = ifaddr, rcnt = 0; ifa != NULL && rcnt < max_cnt;
		ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL || AF_INET != ifa->ifa_addr->sa_family)
			continue;

		ip_h = ntohl(((struct sockaddr_in *) ifa->ifa_addr)->sin_addr.s_addr);
		if (filter(ip_h)) {
			util_ip2str_r(ip_h, ipsr[rcnt], max_len);
			rcnt++;
		}
	}

	freeifaddrs(ifaddr);
	*cnt = rcnt;
	return 0;
}

int
util_get_ip(uint32_t *ip, int max_cnt, int *cnt,
boolean(*filter) (uint32_t ip))
{
	struct ifaddrs *ifaddr, *ifa;
	int rcnt = 0;
	uint32_t ip_h = 0;

	if (getifaddrs(&ifaddr) == -1) {
		return errno;
	}

	/* Walk through linked list, maintaining head pointer so we
	*               can free list later */

	for (ifa = ifaddr, rcnt = 0; ifa != NULL && rcnt < max_cnt;
		ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL || AF_INET != ifa->ifa_addr->sa_family)
			continue;

		ip_h = ntohl(((struct sockaddr_in *) ifa->ifa_addr)->sin_addr.s_addr);
		if (filter(ip_h)) {
            ip[rcnt] = ip_h;
			rcnt++;
		}
	}

	freeifaddrs(ifaddr);
	*cnt = rcnt;
	return 0;
}


int
util_create_pipe(int pipe_fd[2])
{
	int ret;
	int my_fd[2] = { -1, -1 };
	ret = pipe(my_fd);
	if (0 != ret)
		return -1;

	ret = util_set_nonblock(my_fd[0], 1);
	if (0 != ret)
		goto failed;
	util_set_cloexec(my_fd[0], 1);

	ret = util_set_nonblock(my_fd[1], 1);
	if (0 != ret)
		goto failed;
	util_set_cloexec(my_fd[1], 1);

	pipe_fd[0] = my_fd[0];
	pipe_fd[1] = my_fd[1];

	return 0;
failed:
	close(my_fd[0]);
	my_fd[0] = -1;

	close(my_fd[1]);
	my_fd[1] = -1;

	return -1;
}

int
util_create_listen_fd(const char *ip, unsigned short port, int backlog)
{
	int ret;
	int fd = -1;
	int val;
	struct linger lg;
	struct sockaddr_in addr;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == fd) {
		ret = -1;
		goto failed;
	}

	addr.sin_family = AF_INET;
	if (0 == inet_aton(ip, &(addr.sin_addr))) {
		ret = -2;
		goto failed;
	}
	addr.sin_port = htons(port);

	ret = fcntl(fd, F_SETFD, FD_CLOEXEC);
	if (-1 == ret) {
		ret = -3;
		goto failed;
	}

	ret = fcntl(fd, F_GETFL);
	if (-1 == ret) {
		ret = -4;
		goto failed;
	}

	ret = fcntl(fd, F_SETFL, ret | O_NONBLOCK);
	if (0 != ret) {
		ret = -5;
		goto failed;
	}

	val = 1;
	ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
	if (0 != ret) {
		ret = -6;
		goto failed;
	}

	lg.l_onoff = 0;
	lg.l_linger = 0;
	ret = setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));

	ret = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
	if (0 != ret) {
		ERR("bind addr failed. error = %s", strerror(errno));
		ret = -7;
		goto failed;
	}

	ret = listen(fd, backlog);
	if (0 != ret) {
		ret = -8;
		goto failed;
	}

	return fd;
failed:
	if (-1 != fd)
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

	if (-1 == fd)
		return -1;

	ret = fcntl(fd, F_SETFD, FD_CLOEXEC);
	if (-1 == ret)
		goto failed;

	ret = fcntl(fd, F_GETFL);
	if (-1 == ret)
		goto failed;

	ret = fcntl(fd, F_SETFL, ret | O_NONBLOCK);
	if (0 != ret)
		goto failed;

	val = buf_sz;
	ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
	if (0 != ret)
		WRN("can't do setsockopt(SO_RCVBUF) for fd %d.error:%d.", fd, errno);
	ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
	if (0 != ret)
		WRN("can't do setsockopt(SO_SNDBUF) for fd %d.error:%d.", fd, errno);

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(ip);
	addr.sin_port = htons(port);
	ret = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
	if (0 != ret)
		goto failed;

	return fd;

failed:
	if (-1 != fd)
		close(fd);
	return -1;
}

int
util_create_passive_fd(unsigned short port)
{
	int fd;
	int ret;
	struct sockaddr_in addr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == fd)
		goto failed;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(port);
	ret = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
	if (0 != ret)
		goto failed;

	ret = fcntl(fd, F_SETFD, FD_CLOEXEC);
	if (-1 == ret)
		goto failed;

	ret = fcntl(fd, F_GETFL);
	if (-1 == ret)
		goto failed;

	ret = fcntl(fd, F_SETFL, ret | O_NONBLOCK);
	if (0 != ret)
		goto failed;

	ret = fcntl(fd, F_SETFD, FD_CLOEXEC);
	if (0 != ret)
		goto failed;

	return fd;

failed:
	if (-1 != fd)
		close(fd);
	return -1;
}


int
util_fire_passive_fd(int fd, unsigned short port)
{
	struct sockaddr_in addr;
	int data = 0;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(port);
	return sendto(fd, &data, sizeof(data), 0, (struct sockaddr *) &addr,
		sizeof(addr));
}

int
util_set_limit(int resource, rlim_t cur, rlim_t max)
{
	if (getuid() == 0) {
		struct rlimit rlim;

		if (getrlimit(resource, &rlim) == 0) {
			/* set max file number to 40k */
			rlim.rlim_cur = cur;
			rlim.rlim_max = max;
			if (0 != setrlimit(RLIMIT_NOFILE, &rlim)) {
				ERR("can't set 'max filedescriptors': %s", strerror(errno));
				return 1;
			}
		}
	}
	return 0;
}

int
util_unmask_cpu0()
{
	size_t masksize;
	uint64_t cpumask = 0;

	masksize = sizeof(cpumask);
	if ((sched_getaffinity(0, masksize, (cpu_set_t *)& cpumask) == 0)
		&& (cpumask >= 3)) {
		/* at least two cpu */
		if (cpumask & 0x1) {
			/* mask CPU0
			* CPU0 is used to process network interrupts
			*/
			--cpumask;
			if (sched_setaffinity(0, masksize, (cpu_set_t *)& cpumask)) {
				WRN("couldn't sched_setaffinity to mask 0x%x", (int)cpumask);
				return -1;
			}
		}
	}
	return 0;
}


unsigned long long
util_ntohll(unsigned long long val)
{
	if (__BYTE_ORDER == __LITTLE_ENDIAN) {
		return (((unsigned long long) htonl((int)((val << 32) >> 32))) <<
			32) | (unsigned int)htonl((int)(val >> 32));
	}
	else if (__BYTE_ORDER == __BIG_ENDIAN) {
		return val;
	}
}

unsigned long long
util_htonll(unsigned long long val)
{
	if (__BYTE_ORDER == __LITTLE_ENDIAN) {
		return (((unsigned long long) htonl((int)((val << 32) >> 32))) <<
			32) | (unsigned int)htonl((int)(val >> 32));
	}
	else if (__BYTE_ORDER == __BIG_ENDIAN) {
		return val;
	}
}

#define MAX_PARA_LEN ((size_t)128)
#define MAX_KEY_LEN ((size_t)32)
int
util_gen_token(time_t time_val, uint32_t streamid,
const char *private_key, size_t key_len,
const char *para1, size_t para1_len,
const char *para2, size_t para2_len,
char *token, size_t token_len)
{
	int ret;

	char buf[128];

	if (token_len <= 32)
		return -1;

	MD5_CTX md5_ctx;

	MD5_Init(&md5_ctx);

	char ts_buf[128];

	ret = snprintf(ts_buf, sizeof(ts_buf), "%08x",
		(unsigned int)(time_val & 0xFFFFFFFF));
	MD5_Update(&md5_ctx, ts_buf, ret);

	ret = snprintf(buf, sizeof(buf), "%u", streamid);
	MD5_Update(&md5_ctx, buf, ret);

	MD5_Update(&md5_ctx, private_key, util_min(key_len, MAX_KEY_LEN));
	MD5_Update(&md5_ctx, para1, util_min(para1_len, MAX_PARA_LEN));
	MD5_Update(&md5_ctx, para2, util_min(para2_len, MAX_PARA_LEN));

	MD5_Final_str(buf, sizeof(buf), &md5_ctx);

	memset(token, 0, token_len);
	memcpy(token, ts_buf, 8);
	memcpy(token + 8, buf + 8, 32 - 8);

	return 0;
}

#define TOKEN_TIMEOUT (12 * 60 * 60)

boolean
util_check_token(time_t curr_time, uint32_t streamid,
const char *private_key, size_t key_len,
const char *para1, size_t para1_len,
const char *para2, size_t para2_len,
unsigned int token_timeout, const char *token)
{
	time_t time_val = 0;
	size_t token_len = strlen(token);
	char buf[64];

	if (32 != token_len)
		return FALSE;

	if (!util_check_hex(token, token_len))
		return FALSE;

	memcpy(buf, token, 8);
	buf[8] = 0;
	curr_time &= 0xFFFFFFFF;
	time_val = strtol(buf, NULL, 16);
	if (labs(curr_time - time_val) >= token_timeout)
		return FALSE;

	memset(buf, 0, sizeof(buf));
	util_gen_token(time_val, streamid, private_key, key_len,
		para1, para1_len, para2, para2_len, buf, sizeof(buf));

	if (0 == strcmp(buf, token))
		return TRUE;
	return FALSE;
}

boolean
util_check_hex(const char *ptr, size_t len)
{
	const char *p = ptr;

	if (0 == len)
		return FALSE;

	while (len-- != 0 && '\0' != *p) {
		if (!isxdigit(*p++))
			return FALSE;
	}
	return TRUE;
}

boolean
util_check_digit(const char *ptr, size_t len)
{
	size_t pos = 0;

	for (; pos < len; pos++) {
		if (!isdigit(ptr[pos]))
			return FALSE;
	}
	return TRUE;
}

int
util_hex_dump(const void *ptr, size_t len, char *buf, size_t buf_len)
{
	if (buf_len < len * 2 + 1)
		return -1;
	const char *ptr_c = (const char *)ptr;
	size_t i = 0;

	for (; i < len; i++)
		sprintf(buf + i * 2, "%02x", ptr_c[i]);
	buf[buf_len - 1] = '\0';
	return 0;
}

void
util_http_rsp_error_code(int fd, http_code code)
{
	//HTTP/1.0 404 Not Found\r\nConnection: close\r\n\r\n
	if (code >= HTTP_START && code < HTTP_END) {
		char rsp_line[1024];

		memset(rsp_line, 0, sizeof(rsp_line));
		snprintf(rsp_line, sizeof(rsp_line),
			"HTTP/1.0 %s %s\r\nConnection: close\r\n\r\n",
			g_http_code[code][0], g_http_code[code][1]);
		int total = 0;
		int ret = 0;
		size_t len = strlen(rsp_line);

        while (total < (int32_t)len) {
			ret = write(fd, rsp_line + total, len - total);
			if (ret < 0) {
				if (EAGAIN == errno || EWOULDBLOCK == errno)
					break;
				if (EINTR != errno)
					break;
			}
			else if (0 == ret) {
				break;
			}
			else {
				total += ret;
			}
		}
	}
}

const char *
util_http_code2numstr(http_code code)
{
	if (code < HTTP_START || code >= HTTP_END)
		return NULL;

	return g_http_code[code][0];
}

const char *
util_http_code2str(http_code code)
{
	if (code < HTTP_START || code >= HTTP_END)
		return NULL;

	return g_http_code[code][1];
}

int
util_create_pid_file(const char *path, const char *process_name)
{
	char file_path[PATH_MAX];
	char real_path[PATH_MAX];
	char content[32];
	int fd = -1;
	int ret = -1;
	int len = -1;

	memset(file_path, 0, sizeof(file_path));
	memset(real_path, 0, sizeof(real_path));
	memset(content, 0, sizeof(content));

    INF("create pid file at %s", path);

	if (NULL == realpath(path, real_path))
		return errno;

	ret =
		snprintf(file_path, sizeof(file_path)-1, "%s/%s.pid", real_path,
		process_name);

    INF("create pid file: %s", file_path);

    if (ret < 0 || ret >= (int32_t)sizeof(file_path)-1)
		return -1;

	fd = open(file_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (-1 == fd)
		return errno;

	len = snprintf(content, sizeof(content)-1, "%d", (int)getpid());
	if (len < 0 || len >= (int)sizeof(content)-1) {
		close(fd);
		return -2;
	}

	ret = write(fd, content, len);
	if (-1 == ret) {
		close(fd);
		unlink(file_path);
		return errno;
	}

	if (ret < len) {
		close(fd);
		unlink(file_path);
		return -3;
	}

	close(fd);
	return 0;
}
#ifndef _WIN32
void itoa(int value, char *str)
{
        if (value < 0)
        {
                str[0] = '-';
                value = 0-value;
        }
        int i,j;
        for(i=1; value > 0; i++,value/=10)
                str[i] = value%10+'0';
        for(j=i-1,i=1; j-i>=1; j--,i++)
        {
                str[i] = str[i]^str[j];
        str[j] = str[i]^str[j];
                str[i] = str[i]^str[j];
        }
        if(str[0] != '-')
        {
                for(i=0; str[i+1]!='\0'; i++)
                      str[i] = str[i+1];
                str[i] = '\0';
    }
}
#endif
