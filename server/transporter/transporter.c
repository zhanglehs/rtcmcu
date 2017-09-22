/* sophiscated FLV stream transporter for Youku.com
 *
 * features:
 * 1) ability to seek to newest keyframe if client lag behind
 * 2) use two memory segment to save flv stream data
 * 3) ability to send onCuePoint when client's speed changed
 * 4) ability to switch from high bps stream to middle bps stream automaticly
 *
 * config xml:
	<server.port></server.port>
	<server.maxfds></server.maxfds>
	<server.bind></server.bind>

	<server.control-ip></server.control-ip>
	<server.control-port></server.control-port>
	<server.supervisor></server.supervisor>

	<server.live-buffer-time></server.live-buffer-time>

	<server.logfile></server.logfile>
	<server.max-buffer-unused-size></server.max-buffer-unused-size>

	<report.server></report.server>
	<report.interval></report.interval>

	<client.max-lag-seconds></client.max-lag-seconds>
	<client.seek-to-keyframes></client.seek-to-keyframe>

	<client.expire-time></client.expire-time>

	<client.write-blocksize></client.write-blocksize>
 *
 * Que Hongyu
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <getopt.h>
#include <event.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#ifdef HAVE_SCHED_GETAFFINITY
#include <sched.h>
#endif

#include <sys/ioctl.h>
#include <signal.h>

#include "xml.h"
#include "transporter.h"

struct config conf;

int mainfd = -1, to_daemon = 1, verbose_mode = 0;
struct connection *client_conns = NULL;

/* control.c */
extern int setup_control_server(void);
extern void remove_live_stream_timer(void);
extern int init_global_backends(void);
extern void free_global_backends(void);

/* client.c */
extern void main_service(void);
extern struct event_base *main_base;

/* backend.c */
extern void free_segment(struct segment *);
extern void close_live_stream(struct stream *);

/* ------- */
time_t cur_ts;
char cur_ts_str[128];

static const char resivion[] __attribute__((used)) = { "$Id: transporter.c 496 2011-01-14 03:24:26Z qhy $" };

static void
show_help(void)
{
	fprintf(stderr, "Youku Flash Live Transporter v%s, Build-Date: " __DATE__ " " __TIME__ "\n"
		  "Usage:\n"
		  " -h this message\n"
		  " -c file, config file, default is /etc/flash/transporter.xml\n"
		  " -D don't go to background\n"
		  " -v verbose\n\n", VERSION);
}

/* --- code from freebsd --- */
 /*
  *   Support for variable-length addresses.
  */
#ifdef _SIZEOF_ADDR_IFREQ
#define NEXT_INTERFACE(ifr) ((struct ifreq *) \
        ((char *) ifr + _SIZEOF_ADDR_IFREQ(*ifr)))
#define IFREQ_SIZE(ifr) _SIZEOF_ADDR_IFREQ(*ifr)
#else
#ifdef HAS_SA_LEN
#define NEXT_INTERFACE(ifr) ((struct ifreq *) \
        ((char *) ifr + sizeof(ifr->ifr_name) + ifr->ifr_addr.sa_len))
#define IFREQ_SIZE(ifr) (sizeof(ifr->ifr_name) + ifr->ifr_addr.sa_len)
#else
#define NEXT_INTERFACE(ifr) (ifr + 1)
#define IFREQ_SIZE(ifr) sizeof(ifr[0])
#endif
#endif

static int
get_internalip(char *result)
{
	struct ifconf ifc;
	struct ifreq *ifr;
	struct ifreq *the_end;
	struct in_addr addr;
	int s, len;
	char *p;

	if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
		return 0;

	len = 1024;
	if ((p = (char *)malloc(len)) == NULL) {
		close(s);
		return 0;
	}

	for (;;) {
		ifc.ifc_len = len;
		ifc.ifc_buf = p;
		if (ioctl(s, SIOCGIFCONF, (char *) &ifc) < 0) {
			if (errno != EINVAL) {
				free(p);
				close(s);
				return 0;
			}
		} else if (ifc.ifc_len < len/2)
			break;
		free(p);
		len *= 2;
		if ((p = (char *)malloc(len)) == NULL) {
			close(s);
			return 0;
		}
	}

	the_end = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
	for (ifr = ifc.ifc_req; ifr < the_end;) {
		if (ifr->ifr_addr.sa_family == AF_INET) {       /* IP interface */
			if (ioctl(s, SIOCGIFFLAGS, (char *) ifr) != -1) {
				if ((ifr->ifr_flags & IFF_UP) == 0 ||
						(ifr->ifr_flags & IFF_LOOPBACK) ||
						(ifr->ifr_flags & (IFF_BROADCAST | IFF_POINTOPOINT)) == 0) {
				} else {
					addr = ((struct sockaddr_in *) & ifr->ifr_addr)->sin_addr;
					if (addr.s_addr != INADDR_ANY && ((strncmp(inet_ntoa(addr), "10.", 3) == 0 ||
							strncmp(inet_ntoa(addr), "192.168.", 8) == 0))) {
						strcpy(result, inet_ntoa(addr));
						free(p);
						close(s);
						return 1;
					}
				}
			}
		}
		ifr = NEXT_INTERFACE(ifr);
	}

	free(p);
	close(s);
	return 0;
}

static int
parseconf(char *file)
{
	struct xmlnode *mainnode, *config, *p;
	int status = 1;

	if (file == NULL || file[0] == '\0') return 1;

	mainnode = xmlloadfile(file);
	if (mainnode == NULL) return 1;

	config = xmlgetchild(mainnode, "config", 0);

	conf.port = 81;
	conf.maxfds = 8192;
	conf.admin_ip[0] = '\0';
	conf.bindhost[0] = '\0';
	conf.stream_ip[0] = '\0';
	conf.admin_port = 5500;
	conf.client_max_lag_seconds = 0;
	conf.seek_to_keyframe = 1;
	conf.logfp = stderr;
	conf.live_buffer_time = 20 * 60; /* 20 minutes */
	conf.report_fd = 0;
	conf.client_write_blocksize = 8192;
	conf.report_interval = 60; /* 1 minute */
	conf.client_expire_time = 0; /* default is not to close client */
	conf.max_backend_retry_count = 40; /* default retry connection to backend server for 40 times */

	memset(&(conf.supervisor_server), 0, sizeof(conf.supervisor_server));
	memset(&(conf.report_server), 0, sizeof(conf.report_server));

	if (config) {
		p = xmlgetchild(config, "server.port", 0);
		if (p && p->value != NULL) {
			conf.port = atoi(p->value);
			if (conf.port <= 0) conf.port = 81;
		}

		p = xmlgetchild(config, "server.maxfds", 0);
		if (p && p->value != NULL) {
			conf.maxfds = atoi(p->value);
			if (conf.maxfds <= 0) conf.maxfds = 8192;
		}

		p = xmlgetchild(config, "server.stream-ip", 0);
		if (p && p->value != NULL)
			strncpy(conf.stream_ip, p->value, 32);

		p = xmlgetchild(config, "server.control-ip", 0);
		if (p && p->value != NULL)
			strncpy(conf.admin_ip, p->value, 32);

		p = xmlgetchild(config, "server.control-port", 0);
		if (p && p->value != NULL) {
			conf.admin_port = atoi(p->value);
			if (conf.admin_port <= 0) conf.admin_port = 5500;
		}

		p = xmlgetchild(config, "server.bind", 0);
		if (p && p->value != NULL)
			strncpy(conf.bindhost, p->value, 32);

		p = xmlgetchild(config, "server.live-buffer-time", 0);
		if (p && p->value != NULL) {
			conf.live_buffer_time = atoi(p->value);
			if (conf.live_buffer_time < 0) conf.live_buffer_time = 20 * 60; /* 20 minutes */
		}

		p = xmlgetchild(config, "server.max-retry-count", 0);
		if (p && p->value != NULL) {
			conf.max_backend_retry_count = atoi(p->value);
			if (conf.max_backend_retry_count < 0) conf.max_backend_retry_count = 40;
		}

		p = xmlgetchild(config, "server.supervisor", 0);
		if (p && p->value != NULL) {
			char *q;
			conf.supervisor_server.sin_family = AF_INET;
			q = strchr(p->value, ':');
			if (q) {
				*q = '\0';
				q ++;
			}

			conf.supervisor_server.sin_addr.s_addr = inet_addr(p->value);
			conf.supervisor_server.sin_port = htons(q == NULL?5500:atoi(q));

			status = 0;
		} else {
			ERROR_LOG(stderr, "no supervisor_server section in %s\n", file);
		}

		p = xmlgetchild(config, "server.logfile", 0);
		if (p && p->value != NULL) {
			conf.logfp = fopen(p->value, "a");
			if (conf.logfp == NULL) {
				ERROR_LOG(stderr, "can't open logfile %s\n", p->value);
				conf.logfp = stderr;
			}
		}

		/* report related config options */
		p = xmlgetchild(config, "report.server", 0);
		if (p && p->value != NULL) {
			char *q;
			conf.report_server.sin_family = AF_INET;
			q = strchr(p->value, ':');
			if (q) {
				*q = '\0';
				q ++;
			}

			conf.report_server.sin_addr.s_addr = inet_addr(p->value);
			conf.report_server.sin_port = htons(q == NULL?5600:atoi(q));
			conf.report_fd = socket(AF_INET, SOCK_DGRAM, 0);
			if (conf.report_fd < 0) {
				ERROR_LOG(stderr, "can't open udp socket\n");
			}
		}

		p = xmlgetchild(config, "report.interval", 0);
		if (p && p->value != NULL) {
			conf.report_interval = atoi(p->value);
			if (conf.report_interval <= 0) conf.report_interval = 120;
		}

		/* client related config options */
		p = xmlgetchild(config, "client.max-lag-seconds", 0);
		if (p && p->value != NULL) {
			conf.client_max_lag_seconds = atoi(p->value);
			if (conf.client_max_lag_seconds < 0) conf.client_max_lag_seconds = 3;
		}
		conf.client_max_lag_seconds *= 1000;

		p = xmlgetchild(config, "client.seek-to-keyframe", 0);
		if (p && p->value != NULL) {
			conf.seek_to_keyframe = atoi(p->value);
			if (conf.seek_to_keyframe < 0) conf.seek_to_keyframe = 0;
		}

		p = xmlgetchild(config, "client.write-blocksize", 0);
		if (p && p->value != NULL) {
			conf.client_write_blocksize = atoi(p->value);
			if (conf.client_write_blocksize <= 0) conf.client_write_blocksize = 8192;
		}

		p = xmlgetchild(config, "client.expire-time", 0);
		if (p && p->value != NULL) {
			conf.client_expire_time = atoi(p->value);
			if (conf.client_expire_time < 0) conf.client_expire_time = 0;
		}
	}

	freexml(mainnode);

	client_conns = (struct connection *)calloc(sizeof(struct connection), conf.maxfds);
	if (client_conns == NULL) {
		ERROR_LOG(stderr, "out of memory for connection structures\n");
		status = 1;
	}

	return status;
}

void
#if defined(__FreeBSD__)
server_exit(int sig)
#else
server_exit(void)
#endif
{
	int i;
	struct connection *c;

	if (mainfd > 0) close(mainfd);
	if (client_conns) {
		for(i = 0; i < conf.maxfds; i ++) {
			c = client_conns + i;
			if (c->fd > 0) {
				event_del(&(c->ev));
				close(c->fd);
			}
			if (c->r) buffer_free(c->r);
			if (c->w) buffer_free(c->w);
		}

		free(client_conns);
	}

	remove_live_stream_timer();

	free_global_backends();

	exit(0);
}

int
main(int argc, char **argv)
{
	int c;
	char *configfile = "/etc/flash/transporter.xml";
	struct sockaddr_in server;

	while(-1 != (c = getopt(argc, argv, "hDvc:"))) {
		switch(c)  {
		case 'D':
			to_daemon = 0;
			break;
		case 'v':
			verbose_mode = 1;
			break;
		case 'c':
			configfile = optarg;
			break;
		case 'h':
		default:
			show_help();
			return 1;
		}
	}

	if (configfile == NULL || configfile[0] == '\0') {
		fprintf(stderr, "Please provide -c file argument, or use default '/etc/flash/transporter.xml' \n");
		show_help();
		return 1;
	}

	cur_ts = time(NULL);
	strftime(cur_ts_str, 127, "%Y-%m-%d %H:%M:%S", localtime(&cur_ts));

	if (parseconf(configfile)) {
		ERROR_LOG(stderr, "parse xml %s failed\n", configfile);
		return 1;
	}

	if (conf.admin_ip[0] == '\0')
		get_internalip(conf.admin_ip);

	memset((char *) &server, 0, sizeof(server));
	server.sin_family = AF_INET;
	if (conf.bindhost[0] == '\0')
		server.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		server.sin_addr.s_addr = inet_addr(conf.bindhost);

	server.sin_port = htons(conf.port);

	if (getuid() == 0) {
		struct rlimit rlim;
		if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
			/* set max file number to 40k */
			rlim.rlim_cur = 40000;
			rlim.rlim_max = 40000;
			if (0 != setrlimit(RLIMIT_NOFILE, &rlim)) {
				ERROR_LOG(stderr, "can't set 'max filedescriptors': %s\n", strerror(errno));
				return 1;
			}
		}
	}

#ifdef HAVE_SCHED_GETAFFINITY
	{
		size_t masksize;
		uint64_t cpumask = 0;
		masksize = sizeof(cpumask);
		if ((sched_getaffinity(0, masksize, (cpu_set_t *)&cpumask) == 0) && (cpumask >= 3)) {
			/* at least two cpu */
			if (cpumask & 0x1) {
				/* mask CPU0
				 * CPU0 is used to process network interrupts
				 */
				-- cpumask;
				if (sched_setaffinity(0, masksize, (cpu_set_t *)&cpumask))
					ERROR_LOG(conf.logfp, "couldn't sched_setaffinity to mask 0x%x\n", (int)cpumask);
			}
		}
	}
#endif

	if (init_global_backends()) return 1;

	mainfd = socket(AF_INET, SOCK_STREAM, 0);
	if (mainfd < 0) {
		ERROR_LOG(conf.logfp, "can't create network socket\n");
		return 1;
	}

	set_nonblock(mainfd);

	if (bind(mainfd, (struct sockaddr *) &server, sizeof(server)) || listen(mainfd, 512)) {
		ERROR_LOG(conf.logfp, "fd #%d can't bind/listen: %s\n", mainfd, strerror(errno));
		close(mainfd);
		return 1;
	}

#ifdef TCP_DEFER_ACCEPT
	{
		int v = 30; /* 30 seconds */
		if (-1 == setsockopt(mainfd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &v, sizeof(v))) {
			ERROR_LOG(conf.logfp, "can't set TCP_DEFER_ACCEPT on fd #%d\n", mainfd);
		}
	}
#endif

	fprintf(conf.logfp, "%s: Youku Live Transporter v%s (" __DATE__ " " __TIME__ "), HTTPD: TCP %s:%d, CONTROLLER: TCP %s:%d\n",
			cur_ts_str, VERSION, conf.bindhost[0]?conf.bindhost:"0.0.0.0", conf.port,
			conf.admin_ip[0]?conf.admin_ip:"0.0.0.0", conf.admin_port);

	if (to_daemon && daemon(0, 0) == -1) {
		ERROR_LOG(conf.logfp, "failed to be a daemon\n");
		exit(1);
	}

	main_base = event_init();
	if (main_base == NULL) {
		ERROR_LOG(conf.logfp, "can't init libevent\n");
		return 1;
	}

	if (setup_control_server())
		return 1;

	/* ignore SIGPIPE when write to closing fd
	 * otherwise EPIPE error will cause transporter to terminate unexpectedly
	 */
	signal(SIGPIPE, SIG_IGN);
#if defined(__FreeBSD__) || defined(__APPLE__)
	if (to_daemon == 0)
		signal(SIGINT, server_exit);
	signal(SIGTERM, server_exit);
#else
	if (to_daemon == 0)
		signal(SIGINT, (__sighandler_t) server_exit);
	signal(SIGTERM, (__sighandler_t) server_exit);
#endif
	main_service();
#if defined(__FreeBSD__)
	server_exit(0);
#else
	server_exit();
#endif
	return 0;
}
