/* simple and usable global flash supervisor for flash transporters
 *
 * default config file is /etc/flash/supervisor.xml
 * supported udp command: help/status/stopall/startall
 *
 * Que Hongyu
 * 2009-08-14
 */
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
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>

#include "proto.h"
#include "xml.h"

struct buffer
{
	char *ptr;
	int size, used;
};

/* client connection structure */
struct connection
{
	int fd;
	struct event ev;

	int wpos;
};

typedef struct connection conn;

#define TIMER_RESETALL 1
#define TIMER_BACKUP_CHANNEL 2
#define TIMER_RESTORE_CHANNEL 3
#define TIMER_SENDLISTALL 4
#define TIMER_ENABLE_CHANNEL 5
#define TIMER_DISABLE_CHANNEL 6
#define TIMER_RESET_CHANNEL 7

struct timer
{
	int ts; /* second of day since 0:0:00 */
	int action;
	uint64_t uuid;

	int repeat;

	struct timer *next;
};

struct timelist
{
	int start, end;
	struct timelist *next;
};

#define DESCLEN 32

struct conf
{
	struct stream_info s;
	struct sockaddr_in ip_home, ip_bak;
	struct timelist *g;
	int weekday_start, weekday_end;
	int enable;
	char desc[DESCLEN+1]; /* description of this channel */
};

struct config
{
	int port;
	char bindhost[33];
	int udp_port;
	int maxconns;

	int stream_count;
	struct conf *streams;
};

static struct buffer *payload = NULL;
static struct config conf;
static int verbose_mode = 0, cur_conns = 0;
static struct connection *client_conns = NULL;
static time_t cur_ts;
static char cur_ts_str[128];

static char configfile[128];
static time_t configfile_ts = 0;
static int keepalive_interval = 60; /* 1 minutes */

static int auto_timer = 1;

static void supervisor_client_handler(const int, const short, void *);
static void generate_payload(void);
static int reload_supervisorconf(char *);

struct event ev_master, ev_udp, ev_timer;

static struct timer *timer_header = NULL, *timer_tail = NULL;

static int parse_supervisor_timer(char *, int);

static const char resivion[] __attribute__((used)) = { "$Id: supervisor.c 515 2011-03-29 06:10:45Z qhy $" };

static void
show_supervisor_help(void)
{
	fprintf(stderr, "Youku Flash Live System Supervisor v%s, Build-Date: " __DATE__ " " __TIME__ "\n"
		  "Usage:\n"
		  " -h this message\n"
		  " -c file, config file, default is /etc/flash/supervisor.xml\n"
		  " -D don't go to background\n"
		  " -K num, send listall packet every num seconds, default is 600 seconds\n"
		  " -v verbose\n\n", VERSION);
}

static char *
action_name(int cmd)
{
	switch(cmd) {
		case TIMER_RESETALL: return "RESETALL"; break;
		case TIMER_BACKUP_CHANNEL: return "USE BACKUP SERVER"; break;
		case TIMER_RESTORE_CHANNEL: return "USE MAIN SERVER"; break;
		case TIMER_SENDLISTALL: return "SENDLISTALL"; break;
		case TIMER_ENABLE_CHANNEL: return "ENABLE CHANNEL"; break;
		case TIMER_DISABLE_CHANNEL: return "DISABLE CHANNEL"; break;
		case TIMER_RESET_CHANNEL: return "RESET CHANNEL"; break;
		default: return "NO ACTION"; break;
	}
}

static void
conn_close(conn *c)
{
	if (c) {
		if (c->fd > 0) {
			event_del(&(c->ev));
			close(c->fd);
			c->fd = 0;
		}
		c->wpos = 0;
		cur_conns --;
	}
}

static void
set_nonblock(int fd)
{
	int flags = 1;
	struct linger ling = {0, 0};

	if (fd > 0) {
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));
		setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags));
		setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
		fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)|O_NONBLOCK);
	}
}

static void
write_supervisor_client(conn *c)
{
	int r;

	if (c == NULL || c->fd <= 0) return;

	if (c->wpos < payload->used) {
		/* more header data to write */
		r = write(c->fd, payload->ptr + c->wpos, payload->used - c->wpos);
		if (r < 0) {
			if (errno != EINTR && errno != EAGAIN) {
				conn_close(c);
				return;
			}
			r = 0;
		}

		c->wpos += r;
		if (c->wpos == payload->used) {
			/* finish writing */
			event_set(&(c->ev), c->fd, EV_READ|EV_PERSIST, supervisor_client_handler, (void *) c);
			event_add(&(c->ev), 0);
		} else {
			event_set(&(c->ev), c->fd, EV_WRITE|EV_PERSIST, supervisor_client_handler, (void *) c);
			event_add(&(c->ev), 0);
		}
	}
}

static void
supervisor_client_handler(const int fd, const short which, void *arg)
{
	conn *c;
	int r, toread = -1;
	char buf[129];

	c = (conn *)arg;
	if (c == NULL) return;

	if (which & EV_READ) {
		/* get the byte counts of read */
		if (ioctl(c->fd, FIONREAD, &toread)) {
			if (errno != EAGAIN && errno != EINTR) {
				fprintf(stderr, "ioctl FIONREAD on fd #%d failed\n", c->fd);
				conn_close(c);
				return;
			} else {
				toread = -1;
			}
		}

		/* try to detect whether it's eof */
		if (toread == 0) toread = 1;

		if (toread > 0) {
			if (toread > 128)
				toread = 128;
			r = read(fd, buf, toread);
			if ((r == -1 && errno != EINTR && errno != EAGAIN) || r == 0)  {
				conn_close(c);
			}
		}
	} else if (which & EV_WRITE) {
		if (c->wpos < payload->used) {
			/* more header data to write */
			r = write(c->fd, payload->ptr + c->wpos, payload->used - c->wpos);
			if (r < 0) {
				if (errno != EINTR && errno != EAGAIN) {
					conn_close(c);
					return;
				}
				r = 0;
			}
			c->wpos += r;
			if (c->wpos == payload->used) {
				/* finish writing */
				event_del(&(c->ev));
				event_set(&(c->ev), c->fd, EV_READ|EV_PERSIST, supervisor_client_handler, (void *) c);
				event_add(&(c->ev), 0);
			}
		}
	}
}

static void
supervisor_client_accept(const int fd, const short which, void *arg)
{
	conn *c = NULL;
	int newfd, i;
	struct sockaddr_in s_in;
	socklen_t len = sizeof(struct sockaddr_in);

	memset(&s_in, 0, len);
	newfd = accept(fd, (struct sockaddr *) &s_in, &len);
	if (newfd < 0) {
		fprintf(stderr, "fd %d accept() failed\n", fd);
		return ;
	}

	for (i = 0; i < conf.maxconns; i ++) {
		c = client_conns + i;
		if (c->fd <= 0)
			break;
	}

	if (i == conf.maxconns) {
		write(newfd, "FULL OF CONNECTION\n", strlen("FULL OF CONNECTION\n"));
		close(newfd);
		return;
	}

	c->fd = newfd;
	set_nonblock(c->fd);

	cur_conns ++;
	c->wpos = 0;
	write_supervisor_client(c);
}

static void
send_upstream_cmd(char *p, int len)
{
	int i, r;
	conn *c;

	if (p == NULL || len == 0) return;
	for (i = 0; i < conf.maxconns; i ++) {
		c = client_conns + i;
		if (c->fd > 0) {
			c->wpos = 0;
			while(c->wpos < len) {
				r = write(c->fd, p + c->wpos, len - c->wpos);
				if (r < 0) {
					if (errno != EINTR && errno != EAGAIN) {
						conn_close(c);
						return;
					}
					r = 0;
				}
				c->wpos += r;
			}
		}
	}
}

#define BUFLEN 4096
static void
supervisor_udp_handler(const int fd, const short which, void *arg)
{
	struct sockaddr_in server;
	char buf[BUFLEN+1], buf2[BUFLEN+1], buf3[128], tmp[256];
	int r, i, ip, use_backup, success = 0, has_backup = 0;
	uint64_t uuid;
	struct proto_header header;
	socklen_t structlength;
	conn *c;
	struct timer *t;
	struct timelist *g;

	structlength = sizeof(server);
	r = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *) &server, &structlength);
	if (r == -1) return;

	buf[r] = '\0';
	if (buf[r-1] == '\n') buf[--r] ='\0';

	if (strncasecmp(buf, "stat", 4) == 0) {
		r = snprintf(buf2, BUFLEN, "YOUKU Flash Live System Supervisor v%s, NOW: %s\ntotal #%d streams, "
				"#%d transporters, %s\n", VERSION, cur_ts_str, conf.stream_count, cur_conns, auto_timer?"AUTO":"MANUAL");
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
supervisor_stat:
		for (i = 0; i < conf.stream_count; i ++) {
			memcpy(&ip, &(conf.streams[i].ip_home.sin_addr.s_addr), sizeof(ip));
			ip = htonl(ip);

			if (ip == conf.streams[i].s.ip && conf.streams[i].ip_home.sin_port == conf.streams[i].s.port) use_backup = 0;
			else use_backup = 1;

			if (conf.streams[i].ip_bak.sin_port == 0) has_backup = 0;
			else has_backup = 1;

			if (has_backup) {
				if (use_backup) {
					snprintf(tmp, 128, "MAIN %s:%d",
							inet_ntoa(conf.streams[i].ip_home.sin_addr), ntohs(conf.streams[i].ip_home.sin_port));

					snprintf(buf3, 128, "%s, BACKUP [%s:%d]",
							tmp,
							inet_ntoa(conf.streams[i].ip_bak.sin_addr), ntohs(conf.streams[i].ip_bak.sin_port));
				} else {
					snprintf(tmp, 128, "MAIN [%s:%d]",
							inet_ntoa(conf.streams[i].ip_home.sin_addr), ntohs(conf.streams[i].ip_home.sin_port));

					snprintf(buf3, 128, "%s, BACKUP %s:%d",
							tmp,
							inet_ntoa(conf.streams[i].ip_bak.sin_addr), ntohs(conf.streams[i].ip_bak.sin_port));
				}
			} else {
				snprintf(buf3, 128, "MAIN [%s:%d], NO BACKUP",
						inet_ntoa(conf.streams[i].ip_home.sin_addr), ntohs(conf.streams[i].ip_home.sin_port));
			}

			if (conf.streams[i].g) {
				char tmp2[128];
				tmp[0] = '\0';
				if (conf.streams[i].weekday_start > 0 && conf.streams[i].weekday_end > 0) {
					snprintf(tmp, 128, "W%d-W%d ", conf.streams[i].weekday_start, conf.streams[i].weekday_end);
				}
				g = conf.streams[i].g;
				while(g) {
					snprintf(tmp2, 128, "%02d:%02d->%02d:%02d",
						g->start/3600, (g->start%3600)/60,
						g->end/3600, (g->end%3600)/60);
					strcat(tmp, tmp2);
					g = g->next;
					if (g) strcat(tmp, " ");
				}
			} else {
				snprintf(tmp, 128, "00:00->23:59");
			}

			r = snprintf(buf2, BUFLEN, "[%s] [%s] [#%d UUID] STREAM: \"%s\", stream #%d, BPS #%dK, BUFFER %dS, MPEGTS #%d, DEPTH #%d, %s -> %s\n",
					conf.streams[i].enable?"+":"-",
					tmp,
					ntohl(conf.streams[i].s.uuid),
					conf.streams[i].desc[0]?conf.streams[i].desc:"X",
					ntohs(conf.streams[i].s.streamid),
					ntohs(conf.streams[i].s.bps), ntohs(conf.streams[i].s.buffer_minutes)*60,
					conf.streams[i].s.max_ts_count,
					conf.streams[i].s.max_depth,
					buf3, use_backup?"BACKUP":"MAIN");

			sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
		}

		i = 1;
		t = timer_header;
		while(t) {
			r = snprintf(buf2, BUFLEN, "TIMER #%d: TIME %02d:%02d, ACTION: %s -> #%d UUID, %s\n",
					i, t->ts/3600, (t->ts%3600)/60,
					action_name(t->action), ntohl(t->uuid),
					t->repeat?"Repeat":"Once"
					);
			sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
			t = t->next;
			++ i;
		}
	} else if (strncasecmp(buf, "restoreall", 10) == 0) {
		/* restore all stream from backup server to normal server */
		for (i = 0; i < conf.stream_count; i ++) {
			r = 0;
			if (conf.streams[i].ip_bak.sin_port != 0) {
				memcpy(&ip, &(conf.streams[i].ip_bak.sin_addr.s_addr), sizeof(ip));
				ip = htonl(ip);
				if (ip == conf.streams[i].s.ip && conf.streams[i].ip_bak.sin_port == conf.streams[i].s.port) {
					/* switch back to normal server */
					memcpy(&(conf.streams[i].s.ip), &(conf.streams[i].ip_home.sin_addr.s_addr), sizeof(conf.streams[i].s.ip));
					conf.streams[i].s.ip = htonl(conf.streams[i].s.ip);
					conf.streams[i].s.port = conf.streams[i].ip_home.sin_port;
					success = 1;
					r = snprintf(buf2, BUFLEN, "[#%d UUID] STREAM SERVER: BACKUP -> MAIN\n", ntohl(conf.streams[i].s.uuid));
				}
			}
			if (r > 0) sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
		}
	} else if (strncasecmp(buf, "backupall", 9) == 0) {
		/* switch all streams to backup server */
		for (i = 0; i < conf.stream_count; i ++) {
			r = 0;
			if (conf.streams[i].ip_bak.sin_port != 0) {
				memcpy(&ip, &(conf.streams[i].ip_home.sin_addr.s_addr), sizeof(ip));
				ip = htonl(ip);
				if (ip == conf.streams[i].s.ip && conf.streams[i].ip_home.sin_port == conf.streams[i].s.port) {
					/* normal server, switch to backup server */
					memcpy(&(conf.streams[i].s.ip), &(conf.streams[i].ip_bak.sin_addr.s_addr), sizeof(conf.streams[i].s.ip));
					conf.streams[i].s.ip = htonl(conf.streams[i].s.ip);
					conf.streams[i].s.port = conf.streams[i].ip_bak.sin_port;
					success = 1;
					r = snprintf(buf2, BUFLEN, "[#%d UUID] STREAM SERVER: MAIN -> BACKUP\n", ntohl(conf.streams[i].s.uuid));
				}
			}
			if (r > 0) sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
		}
	} else if (r > 7 && strncasecmp(buf, "backup ", 7) == 0) {
		/* backup uuid */
		uuid = atoi(buf + 7);
		uuid = htonl(uuid);
		r = 0;
		success = 1;
		for (i = 0; i < conf.stream_count; i ++) {
			if (conf.streams[i].s.uuid == uuid) {
				/* found match */
				if (conf.streams[i].ip_bak.sin_port == 0) {
					r = snprintf(buf2, BUFLEN, "[#%d UUID] STREAM HAS NO BACKUP SERVER!\n", ntohl(conf.streams[i].s.uuid));
				} else {
					memcpy(&ip, &(conf.streams[i].ip_home.sin_addr.s_addr), sizeof(ip));
					ip = htonl(ip);
					if (ip == conf.streams[i].s.ip && conf.streams[i].ip_home.sin_port == conf.streams[i].s.port) {
						/* normal server, switch to backup server */
						memcpy(&(conf.streams[i].s.ip), &(conf.streams[i].ip_bak.sin_addr.s_addr), sizeof(conf.streams[i].s.ip));
						conf.streams[i].s.ip = htonl(conf.streams[i].s.ip);
						conf.streams[i].s.port = conf.streams[i].ip_bak.sin_port;
						r = snprintf(buf2, BUFLEN, "[#%d UUID] STREAM SERVER: MAIN -> BACKUP\n", ntohl(conf.streams[i].s.uuid));
					} else {
						r = snprintf(buf2, BUFLEN, "[#%d UUID] STREAM ALREADY USES BACKUP SERVER!\n", ntohl(conf.streams[i].s.uuid));
					}
				}
				if (r > 0) sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
				break;
			}
		}
	} else if (r > 8 && strncasecmp(buf, "restore ", 8) == 0) {
		/* restore uuid */
		uuid = atoi(buf + 8);
		uuid = htonl(uuid);
		r = 0;
		success = 1;
		for (i = 0; i < conf.stream_count; i ++) {
			if (conf.streams[i].s.uuid == uuid) {
				/* found match */
				if (conf.streams[i].ip_bak.sin_port == 0) {
					r = snprintf(buf2, BUFLEN, "[#%d UUID] STREAM HAS NO BACKUP SERVER!\n", ntohl(conf.streams[i].s.uuid));
				} else {
					memcpy(&ip, &(conf.streams[i].ip_home.sin_addr.s_addr), sizeof(ip));
					ip = htonl(ip);
					if (ip != conf.streams[i].s.ip || conf.streams[i].ip_home.sin_port != conf.streams[i].s.port) {
						memcpy(&ip, &(conf.streams[i].ip_bak.sin_addr.s_addr), sizeof(ip));
						ip = htonl(ip);
						if (ip == conf.streams[i].s.ip && conf.streams[i].ip_bak.sin_port == conf.streams[i].s.port) {
							/* backup server, switch back to normal server */
							r = snprintf(buf2, BUFLEN, "[#%d UUID] STREAM SERVER: BACKUP -> MAIN\n", ntohl(conf.streams[i].s.uuid));
							memcpy(&(conf.streams[i].s.ip), &(conf.streams[i].ip_home.sin_addr.s_addr), sizeof(conf.streams[i].s.ip));
							conf.streams[i].s.ip = htonl(conf.streams[i].s.ip);
							conf.streams[i].s.port = conf.streams[i].ip_home.sin_port;
						}
					} else {
						r = snprintf(buf2, BUFLEN, "[#%d UUID] STREAM ALREADY USES MAIN SERVER!\n", ntohl(conf.streams[i].s.uuid));
					}
				}
				if (r > 0) sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
				break;
			}
		}
	} else if (r > 7 && strncasecmp(buf, "switch ", 7) == 0) {
		/* switch uuid */
		uuid = atoi(buf + 7);
		uuid = htonl(uuid);
		success = 1;
		for (i = 0; i < conf.stream_count; i ++) {
			if (conf.streams[i].s.uuid == uuid) {
				/* found match */
				if (conf.streams[i].ip_bak.sin_port == 0) {
					r = snprintf(buf2, BUFLEN, "[#%d UUID] STREAM HAS NO BACKUP SERVER!\n", ntohl(conf.streams[i].s.uuid));
				} else {
					memcpy(&ip, &(conf.streams[i].ip_home.sin_addr.s_addr), sizeof(ip));
					ip = htonl(ip);
					if (ip == conf.streams[i].s.ip && conf.streams[i].ip_home.sin_port == conf.streams[i].s.port) {
						/* normal server, switch to backup server */
						memcpy(&(conf.streams[i].s.ip), &(conf.streams[i].ip_bak.sin_addr.s_addr), sizeof(conf.streams[i].s.ip));
						conf.streams[i].s.ip = htonl(conf.streams[i].s.ip);
						conf.streams[i].s.port = conf.streams[i].ip_bak.sin_port;
						r = snprintf(buf2, BUFLEN, "[#%d UUID] STREAM SERVER: MAIN -> BACKUP\n", ntohl(conf.streams[i].s.uuid));
					} else {
						memcpy(&ip, &(conf.streams[i].ip_bak.sin_addr.s_addr), sizeof(ip));
						ip = htonl(ip);
						if (ip == conf.streams[i].s.ip && conf.streams[i].ip_bak.sin_port == conf.streams[i].s.port) {
							/* backup server, switch back to normal server */
							r = snprintf(buf2, BUFLEN, "[#%d UUID] STREAM SERVER: BACKUP -> MAIN\n", ntohl(conf.streams[i].s.uuid));
							memcpy(&(conf.streams[i].s.ip), &(conf.streams[i].ip_home.sin_addr.s_addr), sizeof(conf.streams[i].s.ip));
							conf.streams[i].s.ip = htonl(conf.streams[i].s.ip);
							conf.streams[i].s.port = conf.streams[i].ip_home.sin_port;
						}
					}
				}
				sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
				break;
			}
		}
	} else if (strncasecmp(buf, "reloadconf", 10) == 0) {
		r = reload_supervisorconf(configfile);

		if (r) {
			r = snprintf(buf2, BUFLEN, "RELOAD CONF FROM \"%s\" FAILED!\n", configfile);
			sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
			return;
		}

		r = snprintf(buf2, BUFLEN, "RELOAD CONF FROM \"%s\" SUCCESSED!\n", configfile);
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));

		success = 1;
		goto supervisor_stat;
	} else if (strncasecmp(buf, "stopall", 7) == 0) {
		header.magic[0] = PROTO_MAGIC[0];
		header.magic[1] = PROTO_MAGIC[1];
		header.length = htons(sizeof(header));
		header.stream_count = header.reserved = 0;
		header.method = STOP_ALL_CMD;
		send_upstream_cmd((char *)&header, sizeof(header));

		r = snprintf(buf2, BUFLEN, "send STOP_ALL_CMD to %d clients\n", cur_conns);
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "sendlistall", 11) == 0) {
		success = 1;
		r = snprintf(buf2, BUFLEN, "send LIST_ALL_CMD to %d clients\n", cur_conns);
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "startall", 8) == 0) {
		header.magic[0] = PROTO_MAGIC[0];
		header.magic[1] = PROTO_MAGIC[1];
		header.length = htons(sizeof(header));
		header.stream_count = header.reserved = 0;
		header.method = START_ALL_CMD;
		send_upstream_cmd((char *)&header, sizeof(header));

		r = snprintf(buf2, BUFLEN, "send START_ALL_CMD to %d clients\n", cur_conns);
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "clearall", 8) == 0 || strncasecmp(buf, "resetall", 8) == 0) {
		header.magic[0] = PROTO_MAGIC[0];
		header.magic[1] = PROTO_MAGIC[1];
		header.length = htons(sizeof(header));
		header.stream_count = header.reserved = 0;
		header.method = CLEAR_ALL_CMD;
		send_upstream_cmd((char *)&header, sizeof(header));

		r = snprintf(buf2, BUFLEN, "send CLEAR_ALL_CMD to %d clients\n", cur_conns);
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (r > 6 && strncasecmp(buf, "reset ", 6) == 0) {
		/* enable uuid */
		uuid = atoi(buf+6);
		if (uuid > 255) {
			r = snprintf(buf2, BUFLEN, "RESET UUID %ld > 255\n", uuid);
			sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
		} else {
			header.magic[0] = PROTO_MAGIC[0];
			header.magic[1] = PROTO_MAGIC[1];
			header.length = htons(sizeof(header));
			header.stream_count = 0;
			header.reserved = (uint8_t) uuid; /* use reserved as uuid */
			header.method = RESET_CMD;
			send_upstream_cmd((char *)&header, sizeof(header));

			r = snprintf(buf2, BUFLEN, "send RESET_CMD #%ld UUID to %d clients\n", uuid, cur_conns);
			sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
		}
	} else if (strncasecmp(buf, "cleartimer", 10) == 0) {
		struct timer *t;
		if (timer_header == NULL) {
			r = snprintf(buf2, BUFLEN, "NO TIMER\n");
		} else {
			while (timer_header) {
				t = timer_header->next;
				free(timer_header);
				timer_header = t;
			}

			timer_tail = timer_header;
			r = snprintf(buf2, BUFLEN, "TIMER CLEARED\n");
		}
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "buffertime ", 11) == 0) {
		/* buffertime uuid minutes */
		char *p, *q;
		uint16_t minutes;

		r = 0;
		q = strchr(buf + 11, ' ');
		if (q) {
			p = q + 1;
			*q = '\0';
			uuid = atoi(buf + 11);
			uuid = htonl(uuid);
			minutes = (uint16_t) atoi(p);
			for (i = 0; i < conf.stream_count; i ++) {
				if (conf.streams[i].s.uuid == uuid) {
					/* found match */
					conf.streams[i].s.buffer_minutes = htons(minutes);
					success = 1;
					r = snprintf(buf2, BUFLEN, "[#%d UUID] BUFFER MINUTE -> %u\n", ntohl(uuid), minutes);
					break;
				}
			}
			*q = ' ';
			if (r == 0)
				r = snprintf(buf2, BUFLEN, "TRY BUFFERTIME UUID MINUTES\n");
		} else {
			r = snprintf(buf2, BUFLEN, "TRY BUFFERTIME UUID MINUTES\n");
		}
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "mpegts ", 7) == 0) {
		/* mpegts uuid ts_number */
		char *p, *q;
		uint8_t number;

		r = 0;
		q = strchr(buf + 7, ' ');
		if (q) {
			p = q + 1;
			*q = '\0';
			uuid = atoi(buf + 7);
			uuid = htonl(uuid);
			number = (uint8_t) atoi(p);
			for (i = 0; i < conf.stream_count; i ++) {
				if (conf.streams[i].s.uuid == uuid) {
					/* found match */
					conf.streams[i].s.max_ts_count = number;
					success = 1;
					r = snprintf(buf2, BUFLEN, "[#%d UUID] MPEGTS NUMBER -> %u\n", ntohl(uuid), number);
					break;
				}
			}
			*q = ' ';
			if (r == 0)
				r = snprintf(buf2, BUFLEN, "TRY MPEGTS UUID NUMBER\n");
		} else {
			r = snprintf(buf2, BUFLEN, "TRY MPEGTS UUID NUMBER\n");
		}
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "depth ", 6) == 0) {
		/* depth uuid number */
		char *p, *q;
		uint8_t number;

		r = 0;
		q = strchr(buf + 6, ' ');
		if (q) {
			p = q + 1;
			*q = '\0';
			uuid = atoi(buf + 6);
			uuid = htonl(uuid);
			number = (uint8_t) atoi(p);
			for (i = 0; i < conf.stream_count; i ++) {
				if (conf.streams[i].s.uuid == uuid) {
					/* found match */
					conf.streams[i].s.max_depth = number;
					success = 1;
					r = snprintf(buf2, BUFLEN, "[#%d UUID] DEPTH -> %u\n", ntohl(uuid), number);
					break;
				}
			}
			*q = ' ';
			if (r == 0)
				r = snprintf(buf2, BUFLEN, "TRY DEPTH UUID NUMBER\n");
		} else {
			r = snprintf(buf2, BUFLEN, "TRY DEPTH UUID NUMBER\n");
		}
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "repeat ", 7) == 0) {
		if (parse_supervisor_timer(buf + 7, 1) == 0) {
			r = snprintf(buf2, BUFLEN, "%s -> TIMER OK\n", buf + 7);
		} else {
			r = snprintf(buf2, BUFLEN, "TRY: TIME UUID ACTION\n");
		}
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "timer ", 6) == 0) {
		if (parse_supervisor_timer(buf + 6, 0) == 0) {
			r = snprintf(buf2, BUFLEN, "%s -> TIMER OK\n", buf + 6);
		} else {
			r = snprintf(buf2, BUFLEN, "TRY: TIME UUID ACTION\n");
		}
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "noauto", 6) == 0) {
		if (auto_timer == 1) {
			auto_timer = 0;
			r = snprintf(buf2, BUFLEN, "DISABLE AUTOMATIC TIMER\n");
			sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
		}
	} else if (strncasecmp(buf, "auto", 4) == 0) {
		if (auto_timer == 0) {
			auto_timer = 1;
			r = snprintf(buf2, BUFLEN, "ENABLE AUTOMATIC TIMER\n");
			sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
		}
	} else if (r > 7 && strncasecmp(buf, "enable ", 7) == 0) {
		/* enable uuid */
		uuid = htonl(atoi(buf+7));
		for (i = 0; i < conf.stream_count; i ++) {
			if (conf.streams[i].s.uuid == uuid && conf.streams[i].enable == 0) {
				/* re-enable it */
				conf.streams[i].enable = 1;
				success = 1;
				r = snprintf(buf2, BUFLEN, "ENABLED [#%d UUID] STREAM: stream #%d, BPS #%dK\n",
						ntohl(conf.streams[i].s.uuid), ntohs(conf.streams[i].s.streamid), ntohs(conf.streams[i].s.bps));
				sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
			}
		}
	} else if (r > 8 && strncasecmp(buf, "disable ", 8) == 0) {
		uuid = htonl(atoi(buf+8));
		for (i = 0; i < conf.stream_count; i ++) {
			if (conf.streams[i].s.uuid == uuid && conf.streams[i].enable == 1) {
				/* disable it */
				conf.streams[i].enable = 0;
				success = 1;
				r = snprintf(buf2, BUFLEN, "DISABLED [#%d UUID] STREAM: stream #%d, BPS #%dK\n",
						ntohl(conf.streams[i].s.uuid), ntohs(conf.streams[i].s.streamid), ntohs(conf.streams[i].s.bps));
				sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
			}
		}
	} else if (strncasecmp(buf, "help", 4) == 0) {
		r = snprintf(buf2, BUFLEN, "1) stopall startall resetall sendlistall backupall restoreall reloadconf\n"
				"2) stat switch enable disable reset noauto auto timer cleartimer repeat\n"
				"3) config restore backup listall mpegts buffertime depth"
				"\n");
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "config", 6) == 0) {
		int is_first = 1;
		buf2[0] = '\0';
		for (i = 0; i < conf.stream_count; i ++) {
			if (conf.streams[i].g != NULL) continue; /* don't include automatic stream */
			memcpy(&ip, &(conf.streams[i].ip_home.sin_addr.s_addr), sizeof(ip));
			ip = htonl(ip);

			if (ip == conf.streams[i].s.ip && conf.streams[i].ip_home.sin_port == conf.streams[i].s.port) use_backup = 0;
			else use_backup = 1;

			if (conf.streams[i].ip_bak.sin_port == 0) has_backup = 0;
			else has_backup = 1;

			/*uuid:stream:bps:enable:backup:use_backup:desc*/
			if (is_first) {
				snprintf(tmp, 128, "%d:%d:%d:%d:%d:%d:%s:%d",
					ntohl(conf.streams[i].s.uuid), ntohs(conf.streams[i].s.streamid), ntohs(conf.streams[i].s.bps),
					conf.streams[i].enable, has_backup, use_backup, conf.streams[i].desc[0]?conf.streams[i].desc:"0", conf.streams[i].s.max_ts_count);
				is_first = 0;
			} else {
				snprintf(tmp, 128, "|%d:%d:%d:%d:%d:%d:%s:%d",
					ntohl(conf.streams[i].s.uuid), ntohs(conf.streams[i].s.streamid), ntohs(conf.streams[i].s.bps),
					conf.streams[i].enable, has_backup, use_backup, conf.streams[i].desc[0]?conf.streams[i].desc:"0", conf.streams[i].s.max_ts_count);
			}
			strcat(buf2, tmp);
		}
		strcat(buf2, "\n");
		sendto(fd, buf2, strlen(buf2), 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "listall", 7) == 0) {
		int is_first = 1;
		buf2[0] = '\0';
		for (i = 0; i < conf.stream_count; i ++) {
			if (conf.streams[i].enable == 0) continue;
			uuid = ntohl(conf.streams[i].s.uuid);
			if (uuid >= 210) continue; /* don't record 210+ channel */
			if (is_first) {
				snprintf(tmp, 128, "%ld", uuid);
				is_first = 0;
			} else {
				snprintf(tmp, 128, "|%ld", uuid);
			}
			strcat(buf2, tmp);
		}
		strcat(buf2, "\n");
		sendto(fd, buf2, strlen(buf2), 0, (struct sockaddr *) &server, sizeof(server));
	} else {
		sendto(fd, NULL, 0, 0, (struct sockaddr *) &server, sizeof(server));
	}

	if (success == 1) {
		generate_payload();
		for (i = 0; i < conf.maxconns; i ++) {
			c = client_conns + i;
			if (c->fd > 0) {
				c->wpos = 0;
				event_del(&(c->ev));
				write_supervisor_client(c);
			}
		}
	}
}

static void
generate_payload(void)
{
	struct proto_header header;
	int i, j = 0, len = 0;
	struct stream_info s;

	for (i = 0; i < conf.stream_count; i ++) {
		if (conf.streams[i].enable == 1 && conf.streams[i].s.max_depth > 0) j ++;
	}

	memset(&header, 0, sizeof(header));
	header.magic[0] = PROTO_MAGIC[0];
	header.magic[1] = PROTO_MAGIC[1];
	header.method = LIST_ALL_CMD;
	len = j * sizeof(struct stream_info) + sizeof(header);
	header.length = htons(len);
	header.stream_count = htons(j);

	if (payload) {
		free(payload->ptr);
		free(payload);
	}

	payload = (struct buffer *)calloc(sizeof(struct buffer), 1);
	if (payload == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}

	payload->ptr = calloc(1, len + 10);
	if (payload->ptr == NULL) {
		fprintf(stderr, "out of memory\n");
		free(payload);
		exit(1);
	}
	payload->size = len + 10;
	memcpy(payload->ptr, &header, sizeof(header));
	payload->used = sizeof(header);

    fprintf(stdout, "conf.stream_count = %d\n", conf.stream_count);
	for (i = 0; i < conf.stream_count; i ++) {
        fprintf(stdout, "stream.enable = %d, max_depth = %d\n", conf.streams[i].enable, conf.streams[i].s.max_depth);
		if (conf.streams[i].enable == 1 && conf.streams[i].s.max_depth > 0) {
			memcpy(&s, &(conf.streams[i].s), sizeof(s));

			memcpy(payload->ptr + payload->used, &s, sizeof(s));
			payload->used += sizeof(s);
		}
	}
}

static int
parse_supervisorconf(char *file, struct config *dst)
{
	struct xmlnode *mainnode, *config, *p;
	int i, j, status = 1;
	struct stat st;
	char *q, *r;

	if (file == NULL || file[0] == '\0' || dst == NULL) return 1;

	mainnode = xmlloadfile(file);
	if (mainnode == NULL) return 1;

	memset(&st, 0, sizeof(st));
	config = xmlgetchild(mainnode, "config", 0);

	dst->port = 8000; /* default is 5500 */
	dst->bindhost[0] = '\0';
	dst->udp_port = 5000;
	dst->stream_count = 0;
	dst->streams = NULL;
	dst->maxconns = 512;

	if (config) {
		p = xmlgetchild(config, "maxconns", 0);
		if (p && p->value != NULL) {
			dst->maxconns = atoi(p->value);
			if (dst->maxconns <= 0) dst->maxconns = 512;
		}

		p = xmlgetchild(config, "port", 0);
		if (p && p->value != NULL) {
			dst->port = atoi(p->value);
			if (dst->port <= 0) dst->port = 5500;
		}

		p = xmlgetchild(config, "bind", 0);
		if (p && p->value != NULL) {
			strncpy(dst->bindhost, p->value, 32);
		}

		p = xmlgetchild(config, "udp_port", 0);
		if (p && p->value != NULL) {
			dst->udp_port = atoi(p->value);
			if (dst->udp_port <= 0) dst->udp_port = 5000;
		}

		j = xmlnchild(config, "stream");
		if (j > 0) {
			dst->streams = (struct conf *) calloc(sizeof(struct conf), j);
			if (dst->streams == NULL) {
				fprintf(stderr, "out of memory for parse_supervisorconf()\n");
				j = 0;
			} else {
				status = 0;
			}

			for (i = 0; i < j; i ++) {
				struct sockaddr_in ip;

				/* <stream channel='id' bps='bps' server='ip:port' uuid='1'
				   buffer_minutes='xxx' status="1" desc="xxx"/> */
				p = xmlgetchild(config, "stream", i);
				q = xmlgetattr(p, "uuid");
				if (q == NULL) {
					status = 1;
					break;
				}
				dst->streams[i].s.uuid = htonl(atoi(q));

				q = xmlgetattr(p, "channel");
				if (q == NULL) {
					status = 1;
					break;
				}
				dst->streams[i].s.streamid = htons(atoi(q));

				q = xmlgetattr(p, "bps");
				if (q == NULL) {
					status = 1;
					break;
				}
				dst->streams[i].s.bps = htons(atoi(q));

				/* server */
				memset(&ip, 0, sizeof(ip));
				r = xmlgetattr(p, "server");
				if (r) {
					q = strchr(r, ':');
					if (q) {
						*q = '\0';
						q ++;
					}

					ip.sin_addr.s_addr = inet_addr(r);
					ip.sin_port = htons(q == NULL?81:atoi(q)); /* default is 81 */

					memcpy(&(dst->streams[i].s.ip), &(ip.sin_addr.s_addr), sizeof(dst->streams[i].s.ip));
					dst->streams[i].s.ip = htonl(dst->streams[i].s.ip);
					dst->streams[i].s.port = ip.sin_port;

					memcpy(&(dst->streams[i].ip_home), &ip, sizeof(ip));
				} else {
					status = 1;
					break;
				}

				/* backup */
				memset(&ip, 0, sizeof(ip));
				r = xmlgetattr(p, "backup");
				if (r) {
					q = strchr(r, ':');
					if (q) {
						*q = '\0';
						q ++;
					}

					ip.sin_addr.s_addr = inet_addr(r);
					ip.sin_port = htons(q == NULL?81:atoi(q)); /* default is 81 */
					memcpy(&(dst->streams[i].ip_bak), &ip, sizeof(ip));
				}

				/* buffer-minute */
				r = xmlgetattr(p, "buffer-minutes");
				if (r) {
					dst->streams[i].s.buffer_minutes = htons(atoi(r));
				} else {
					dst->streams[i].s.buffer_minutes = 0;
				}

				dst->streams[i].s.max_ts_count = 0;
				dst->streams[i].s.max_depth = 10;
				dst->streams[i].s.reserved_3 = dst->streams[i].s.reserved_4 = 0;
				dst->streams[i].enable = 1;

				q = xmlgetattr(p, "mpegts");
				if (q) dst->streams[i].s.max_ts_count = (uint8_t)(atoi(q)); /* set stream's max_ts_count */

				q = xmlgetattr(p, "depth");
				if (q) dst->streams[i].s.max_depth = (uint8_t)(atoi(q)); /* set stream's max_depth */

				/* status */
				r = xmlgetattr(p, "status");
				if (r) dst->streams[i].enable = atoi(r);

				dst->streams[i].g = NULL;

				dst->streams[i].weekday_start = dst->streams[i].weekday_end = 0;
				/* weekday -> "start-end" */
				r = xmlgetattr(p, "weekday");
				if (r) {
					char *s;
					s = strchr(r, '-');
					if (s != NULL && s[1] <= '9' && s[1] >= '0' ) {
						*s = '\0';
						dst->streams[i].weekday_start = atoi(r);
						dst->streams[i].weekday_end = atoi(s + 1);
						*s = '-';
					}
				}

				/* time -> "start-end start-end... " */
				r = xmlgetattr(p, "time");

				if (r) {
					struct timelist *g, *last_g = NULL;
					char *s1, *s2, *s;

					/* 6:25-8:35 8:55-19:45 */
					q = strtok(r, " ");
					while(q) {
						s = strchr(q, '-');
						if (s == NULL || s[1] > '9' || s[1] < '0' ) {
							/* bad format */
							q = strtok(NULL, " ");
							continue;
						}

						g = (struct timelist *) calloc(1, sizeof(*g));
						if (g == NULL) return 1;

						*s = '\0'; /* terminate 'start' part */

						s1 = strchr(q, ':');
						if (s1) {
							g->start = atoi(s1+1) * 60;
							*s1 = '\0';
						}

						g->start += atoi(q) * 3600;
						if (s1) *s1 = ':';

						s2 = s + 1;

						s1 = strchr(s2, ':');
						if (s1) {
							g->end = atoi(s1+1) * 60;
							*s1 = '\0';
						}

						g->end += atoi(s2) * 3600;
						if (s1) *s1 = ':';

						*s = '-'; /* restore '-' */

						if (dst->streams[i].g == NULL) {
							dst->streams[i].g = last_g = g;
						} else {
							last_g->next = g;
							last_g = g;
						}

						q = strtok(NULL, " "); /* to next gate */
					}
				}

				q = xmlgetattr(p, "desc");
				if (q) {
					strncpy(dst->streams[i].desc, q, DESCLEN);
				} else {
					dst->streams[i].desc[0] = '\0';
				}
			}
			dst->stream_count++;
		}
	}

	freexml(mainnode);

	if (status == 0) {
		/* generate_payload(); */
		memset(&st, 0, sizeof(st));
		if (stat(file, &st) == 0) configfile_ts = st.st_mtime;
	}
    fprintf(stdout, "stream_count = %d, stream0.enable = %d\n", dst->stream_count, dst->streams[0].enable);
	return status;
}

/* return 0 if added, return 1 if bad arguments */
static int
parse_supervisor_timer(char *args, int repeat)
{
	char *p, *q, *r, *s;
	struct timer *t;

	if (args == NULL || args[0] == '\0') return 1;

	/* time uuid action */
	p = args;
	q = strchr(p, ' ');

	if (q == NULL) return 1;

	r = strchr(q+1, ' ');
	if (r == NULL) return 1;

	r ++;
	t = (struct timer *) calloc(sizeof(struct timer), 1);
	if (t == NULL) return 1;

	t->repeat = repeat;
	/* uuid */
	*q = '\0';
	t->uuid = htonl(atoi(q+1));

	/* time */
	s = strchr(p, ':');
	if (s) {
		t->ts = atoi(s+1) * 60;
		*s = '\0';
	} else {
		t->ts = 0;
	}

	t->ts += atoi(p) * 3600;
	if (s) *s = ':';

	*q = ' ';
	/* action */
	if (strcasecmp(r, "resetall") == 0) {
		t->action = TIMER_RESETALL;
	} else if (strcasecmp(r, "usebackupserver") == 0) {
		t->action = TIMER_BACKUP_CHANNEL;
	} else if (strcasecmp(r, "usemainserver") == 0) {
		t->action = TIMER_RESTORE_CHANNEL;
	} else if (strcasecmp(r, "sendlistall") == 0) {
		t->action = TIMER_SENDLISTALL;
	} else if (strcasecmp(r, "enablechannel") == 0) {
		t->action = TIMER_ENABLE_CHANNEL;
	} else if (strcasecmp(r, "disablechannel") == 0) {
		t->action = TIMER_DISABLE_CHANNEL;
	} else if (strcasecmp(r, "resetchannel") == 0) {
		t->action = TIMER_RESET_CHANNEL;
	} else {
		fprintf(stderr, "unknown timer action '\%s\', use default SENDLISTALL\n", r);
		t->action = TIMER_SENDLISTALL;
	}

	t->next = NULL;
	if (timer_header == NULL) {
		timer_header = t;
	} else {
		timer_tail->next = t;
	}
	timer_tail = t;
	return 0;
}

static int
reload_supervisorconf(char *file)
{
	int i;
	struct stat st;
	struct config c;
	struct timelist *g;

	if (file == NULL || file[0] == '\0') return 1;

	memset(&st, 0, sizeof(st));
	if (stat(file, &st) || st.st_mtime == configfile_ts) return 1; /* not changed or file not exist */

	if (parse_supervisorconf(file, &c)) return 1;

	for (i = 0; i < conf.stream_count; i ++) {
		/* free gate list of streams */
		while (conf.streams[i].g) {
			g = conf.streams[i].g->next;
			free(conf.streams[i].g);
			conf.streams[i].g = g;
		}
		conf.streams[i].g = NULL;
	}

	free(conf.streams);

	/* replace global config */
	conf.streams = c.streams;
	conf.stream_count = c.stream_count;

	generate_payload();

	for (i = 0; i < conf.maxconns; i ++) {
		if (client_conns[i].fd > 0) {
			client_conns[i].wpos = 0;
			event_del(&(client_conns[i].ev));
			write_supervisor_client(client_conns + i);
		}
	}
	return 0;
}

static void
do_action(uint64_t uuid, int action)
{
	int i, ip;

	for (i = 0; i < conf.stream_count; i ++) {
		if (conf.streams[i].s.uuid != uuid) continue;

		if (action == TIMER_ENABLE_CHANNEL)
			conf.streams[i].enable = 1;
		else if (action == TIMER_DISABLE_CHANNEL)
			conf.streams[i].enable = 0;
		else if (action == TIMER_RESTORE_CHANNEL) {
			if (conf.streams[i].ip_bak.sin_port != 0) {
				memcpy(&ip, &(conf.streams[i].ip_bak.sin_addr.s_addr), sizeof(ip));
				ip = htonl(ip);
				if (ip == conf.streams[i].s.ip && conf.streams[i].ip_bak.sin_port == conf.streams[i].s.port) {
					/* switch back to normal server */
					memcpy(&(conf.streams[i].s.ip), &(conf.streams[i].ip_home.sin_addr.s_addr), sizeof(conf.streams[i].s.ip));
					conf.streams[i].s.ip = htonl(conf.streams[i].s.ip);
					conf.streams[i].s.port = conf.streams[i].ip_home.sin_port;
				}
			}
		} else if (action == TIMER_BACKUP_CHANNEL) {
			if (conf.streams[i].ip_bak.sin_port != 0) {
				memcpy(&ip, &(conf.streams[i].ip_home.sin_addr.s_addr), sizeof(ip));
				ip = htonl(ip);
				if (ip == conf.streams[i].s.ip && conf.streams[i].ip_home.sin_port == conf.streams[i].s.port) {
					/* normal server, switch to backup server */
					memcpy(&(conf.streams[i].s.ip), &(conf.streams[i].ip_bak.sin_addr.s_addr), sizeof(conf.streams[i].s.ip));
					conf.streams[i].s.ip = htonl(conf.streams[i].s.ip);
					conf.streams[i].s.port = conf.streams[i].ip_bak.sin_port;
				}
			}
		}
	}
}

static void
supervisor_timer_service(const int fd, short which, void *arg)
{
	struct timeval tv;
	struct tm tm;
	int day_diff, changed = 0, i, old, weekday = 0;
	struct timer *t, *tp = NULL;
	struct proto_header header;
	struct timelist *g;
	uint64_t uuid2;

	cur_ts = time(NULL);
	strftime(cur_ts_str, 127, "%Y-%m-%d %H:%M:%S", localtime(&cur_ts));

	tv.tv_sec = 1; tv.tv_usec = 0;
	evtimer_set(&ev_timer, supervisor_timer_service, NULL);
	event_add(&ev_timer, &tv);

	if ((cur_ts % 10) == 0) { /* every 60 seconds */
		memset(&tm, 0, sizeof(tm));
		localtime_r(&cur_ts, &tm);
		day_diff = tm.tm_min * 60 + tm.tm_hour * 3600 + tm.tm_sec;
		weekday = tm.tm_wday;
		if (weekday == 0) weekday = 7; /* 7 -> SUNDAY, 1 -> MONDAY */

		if ((cur_ts % keepalive_interval) == 0) changed = 1; /* send list all packet */

		if (auto_timer == 1) {
			for (i = 0; i < conf.stream_count; i ++) {
				if (conf.streams[i].g == NULL) continue;

				if (conf.streams[i].weekday_end > 0 && conf.streams[i].weekday_start > 0) {
					if (weekday > conf.streams[i].weekday_end ||
						weekday < conf.streams[i].weekday_start) {
						/* disabled, only handle disabled day, leave enabled day controlled by time setting */
						if (conf.streams[i].enable) changed = 1;
						conf.streams[i].enable = 0;
						continue;
					}
				}

				g = conf.streams[i].g;
				old = conf.streams[i].enable;
				conf.streams[i].enable = 0;
				while(g) {
					if (g->end > 0) {
						if (day_diff >= g->start && day_diff < g->end) {
							conf.streams[i].enable = 1;
							break;
						}
					}
					g = g->next;
				}

				if (old != conf.streams[i].enable) changed = 1;
			}
		}

		for (i = 1, t = tp = timer_header; t; ++i) {
			if (day_diff != t->ts) {
				/* SKIP */
				tp = t; t = t->next;
				continue;
			}

			/* matched */
			switch (t->action) {
				case TIMER_RESETALL:
				case TIMER_RESET_CHANNEL:

					uuid2 = ntohl(t->uuid);
					header.magic[0] = PROTO_MAGIC[0];
					header.magic[1] = PROTO_MAGIC[1];
					header.length = htons(sizeof(header));
					header.stream_count = 0;
					if (t->action == TIMER_RESETALL) {
						header.reserved = 0;
						header.method = CLEAR_ALL_CMD;
					} else {
						header.reserved = (uint8_t) uuid2; /* use reserved as uuid */
						header.method = RESET_CMD;
					}
					send_upstream_cmd((char *)&header, sizeof(header));
					break;
				case TIMER_SENDLISTALL:
					changed = 1;
					break;
				case TIMER_RESTORE_CHANNEL:
				case TIMER_ENABLE_CHANNEL:
				case TIMER_DISABLE_CHANNEL:
				case TIMER_BACKUP_CHANNEL:
					do_action(t->uuid, t->action);
					if (verbose_mode)
						fprintf(stderr, "(%s.%d) TIMER #%d: %s -> UUID #%d\n", __FILE__, __LINE__,
							i, action_name(t->action), ntohl(t->uuid));
					changed = 1;
					break;
				default:
					break;
			}

			if (t->repeat == 0) {
				/* remove it from list */
				if (t == timer_header) {
					timer_header = t->next;
					if (timer_header == NULL) timer_tail = NULL;
					tp = t = timer_header;
				} else {
					tp->next = t->next;
					free(t);
					t = tp->next;
				}
			} else {
				tp = t; t = t->next;
			}
		}

		if (changed) {
			generate_payload();

			for (i = 0; i < conf.maxconns; i ++) {
				if (client_conns[i].fd > 0) {
					client_conns[i].wpos = 0;
					event_del(&(client_conns[i].ev));
					write_supervisor_client(client_conns + i);
				}
			}
		}
	}
}

int
main(int argc, char **argv)
{
	int sockfd, c, to_daemon = 1, udpfd;
	struct sockaddr_in server, udpaddr;
	struct timeval tv;

	strcpy(configfile, "/etc/flash/supervisor.xml");
	while(-1 != (c = getopt(argc, argv, "hDvc:K:"))) {
		switch(c)  {
		case 'D':
			to_daemon = 0;
			break;
		case 'v':
			verbose_mode = 1;
			break;
		case 'c':
			strncpy(configfile, optarg, 127);
			break;
		case 'K':
			keepalive_interval = atoi(optarg);
			if (keepalive_interval <= 0) keepalive_interval = 60;
			break;
		case 'h':
		default:
			show_supervisor_help();
			return 1;
		}
	}

	if (configfile[0] == '\0') {
		fprintf(stderr, "Please provide -c file argument, or use default '/etc/flash/supervisor.xml' \n");
		show_supervisor_help();
		return 1;
	}

	if (parse_supervisorconf(configfile, &conf)) {
		fprintf(stderr, "(%s.%d) parse xml %s failed\n", __FILE__, __LINE__, configfile);
		return 1;
	}

	client_conns = (struct connection *)calloc(sizeof(struct connection), conf.maxconns);
	if (client_conns == NULL) {
		fprintf(stderr, "(%s.%d) out of memory for client_conns\n", __FILE__, __LINE__);
		return 1;
	}

	memset((char *) &server, 0, sizeof(server));
	memset((char *) &udpaddr, 0, sizeof(udpaddr));

	server.sin_family = AF_INET;
	if (conf.bindhost[0] == '\0')
		server.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		server.sin_addr.s_addr = inet_addr(conf.bindhost);

	server.sin_port = htons(conf.port);

	memcpy(&udpaddr, &server, sizeof(server));
	udpaddr.sin_port = htons(conf.udp_port);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "can't create tcp socket\n");
		return 1;
	}

	set_nonblock(sockfd);

	if (bind(sockfd, (struct sockaddr *) &server, sizeof(server))) {
		if (errno != EINTR)
			fprintf(stderr, "(%s.%d) can't bind at port %d, err %s\n", __FILE__, __LINE__, conf.port, strerror(errno));
		close(sockfd);
		return 1;
	}

	if (listen(sockfd, 512)) {
		fprintf(stderr, "(%s.%d) errno = %d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		close(sockfd);
		return 1;
	}

	udpfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (udpfd < 0) {
		fprintf(stderr, "can't create udp socket\n");
		return 1;
	}

	set_nonblock(udpfd);
	if (bind(udpfd, (struct sockaddr *) &udpaddr, sizeof(udpaddr))) {
		if (errno != EINTR)
			fprintf(stderr, "(%s.%d) errno = %d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		close(udpfd);
		return 1;
	}

	cur_ts = time(NULL);
	strftime(cur_ts_str, 127, "%Y-%m-%d %H:%M:%S", localtime(&cur_ts));

	fprintf(stderr, "%s: Youku Live Supervisor v%s(" __DATE__ " " __TIME__ "), CONTROLLER: TCP %s:%d, ADMIN: UDP 0.0.0.0:%d\n",
			cur_ts_str, VERSION, conf.bindhost[0]?conf.bindhost:"0.0.0.0", conf.port, conf.udp_port);

	if (to_daemon && daemon(0, 0) == -1) {
		fprintf(stderr, "(%s.%d) failed to be a daemon\n", __FILE__, __LINE__);
		exit(1);
	}

	signal(SIGPIPE, SIG_IGN);

	generate_payload();

	event_init();
	/* tcp */
	event_set(&ev_master, sockfd, EV_READ|EV_PERSIST, supervisor_client_accept, NULL);
	event_add(&ev_master, 0);

	/* udp */
	event_set(&ev_udp, udpfd, EV_READ|EV_PERSIST, supervisor_udp_handler, NULL);
	event_add(&ev_udp, 0);

	/* timer */
	tv.tv_sec = 1; tv.tv_usec = 0;
	evtimer_set(&ev_timer, supervisor_timer_service, NULL);
	event_add(&ev_timer, &tv);

	event_dispatch();
	return 0;
}
