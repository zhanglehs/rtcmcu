/* simple youku flv live system report server for youku.com
 *
 * Que Hongyu
 * 2009-08-16
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

#include "proto.h"

struct binary_node
{
	void *ptr;
	uint32_t key;
};

struct binary_tree
{
	struct binary_node **tree;
	int used, size;
};

static struct binary_node * binarytree_search(struct binary_tree *, uint32_t);
static struct binary_tree * binarytree_insert(struct binary_tree *, uint32_t, void *, int *);
static int binarytree_delete(struct binary_tree *, uint32_t);

static int hashsize, hashmask;
static struct binary_tree **hashmap = NULL;

struct channelstat
{
	uint32_t channel;
	uint32_t bps;
	uint32_t current_online, max_online, visited_users;
	uint32_t total_view_users;
	uint64_t total_view_seconds;
};

struct channelsummary
{
	struct channelstat *s;
	int used, size;
};

struct livestat
{
	uint32_t ip;
	uint32_t port;
	uint32_t bps;
	uint32_t channel;
	uint64_t sid;
	time_t ts, start_ts;

	uint32_t last_cmd;

	struct livestat *next;
};

struct channelstat global_stats;

static time_t visited_ts = 0;

struct channelsummary stream_stats, bps_summary, channel_summary;

static time_t cur_ts, clean_ts;
static char cur_ts_str[128];
struct event ev_timer;

static int expire_time = 240; /* 3 minutes */
static int clean_step, clean_start;

static int verbose_mode = 0, minimum_bps = 0;

static double global_multiply = 1.0;

struct timer
{
	int ts;
	int repeat;
	struct timer *next;
};

static struct timer *reset_header = NULL, *reset_tail = NULL;

static const char resivion[] __attribute__((used)) = { "$Id: reporter.c 509 2011-02-01 06:52:02Z qhy $" };

static int
daemonize(int nochdir, int noclose)
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
	
	if (nochdir == 0) {
		if (chdir("/") != 0) {
			perror("chdir");
			return (-1);
		}
	}
	
	if (noclose == 0 && (fd = open("/dev/null", O_RDWR, 0)) != -1) {
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

static struct binary_node *
binarytree_search(struct binary_tree *p, uint32_t key)
{
	int high, middle, low = 0;
	struct binary_node *n;
	int i;

	if (p == NULL || p->used == 0 || p->tree == NULL) return NULL;

	high = p->used;
	while (low <= high && low < p->used) {
		middle = (high + low) / 2;
		n = p->tree[middle];
		if (n->key == key) {
			if (n->ptr == NULL) { /* make sure that n->ptr is not NULL */
				-- p->used;
				for (i = middle; i < p->used; i ++)
					p->tree[i] = p->tree[i+1];
				p->tree[p->used] = NULL;
				free(n);
				n = NULL;
			}
			return n;
		}
		if (n->key > key) {
			/* to lower */
			high = middle - 1;
		} else {
			/* to upper */
			low = middle + 1;
		}
	}
	return NULL;
}

/* set res = 1 if failed */
static struct binary_tree *
binarytree_insert(struct binary_tree *root, uint32_t key, void *ptr, int *res)
{
	struct binary_node *n;
	struct binary_tree *p;
	int i;

	if (root == NULL) {
		p = (struct binary_tree *) calloc(1, sizeof(struct binary_tree));
		if (p == NULL) {
			if (*res) *res = 1;
			return NULL;
		}
	} else {
		p = root;
	}

	n = binarytree_search(p, key);
	if (n) {
		if (n->ptr == NULL)
			n->ptr = ptr;
		else if (*res) *res = 1;
		return p;
	}

	/* new item */
#define BINARY_STEP 10
	if (p->size == 0) {
		p->tree = (struct binary_node **)calloc(sizeof(struct binary_node*), BINARY_STEP);
		if (p->tree == NULL) {
			if (*res) *res = 1;
			return p;
		}
		p->size = BINARY_STEP;
		p->used = 0;
	} else if (p->used == p->size) {
		void *q;

		q = realloc(p->tree, sizeof(struct binary_node *) * (p->size + BINARY_STEP));
		if (q == NULL) {
			if (*res) *res = 1;
			return p;
		}
		p->tree = (struct binary_node **) q;
		p->size += BINARY_STEP;
	}
#undef BINARY_STEP

	n = (struct binary_node *) calloc(sizeof(struct binary_node), 1);
	if (n == NULL) {
		if (*res) *res = 1;
		return p;
	}
	n->ptr = ptr;
	n->key = key;
	if (p->used == 0) {
		p->tree[p->used] = n;
	} else if (p->used == 1) {
		if (p->tree[0]->key > key) {
			p->tree[1] = p->tree[0];
			p->tree[0] = n;
		} else {
			p->tree[1] = n;
		}
	} else if (p->tree[p->used-1]->key < key) {
		p->tree[p->used] = n;
	} else {
		/* in the middle or the lowest */
		for (i = p->used - 1; i >= 0; --i) {
			if (p->tree[i]->key > key)
				p->tree[i+1] = p->tree[i];
			else
				break;
		}
		p->tree[i+1] = n;
	}
	++ p->used;
	return p;
}

/* return 0 if success
 * return 1 if not found
 */
static int
binarytree_delete(struct binary_tree *p, uint32_t key)
{
	int high, middle, low = 0;
	struct binary_node *n;
	int i;

	if (p == NULL || p->used == 0 || p->tree == NULL) return 1;

	high = p->used;
	while (low <= high && low < p->used) {
		middle = (high + low) / 2;
		n = p->tree[middle];
		if (n->key == key) {
			/* found */
			for (i = middle; i < p->used - 1; i ++) {
				p->tree[i] = p->tree[i+1];
			}
			-- p->used;
			p->tree[p->used] = NULL;
			free(n);
			return 0;
		}
		if (n->key > key) {
			/* to lower part */
			high = middle - 1;
		} else {
			/* to upper part */
			low = middle + 1;
		}
	}
	return 1;
}

static void
show_reporter_help(void)
{
	fprintf(stderr, "Youku Flash Live System Reporter v%s, Build-Date: " __DATE__ " " __TIME__ "\n"
		  "Usage:\n"
		  " -h this message\n"
		  " -D don't go to background\n"
		  " -p port, udp listen port, default is 5600\n"
		  " -m port, udp admin port, default is 5700\n"
		  " -M bps, minimum bps of reporting thresold, default is zero\n"
		  " -l host, bind host, default is ANY\n"
		  " -t expire, item expire time, default is 240 second\n"
		  " -C maxchannels, max recorded channel. default is 4096\n"
		  " -v verbose mode"
		  "\n",
		  VERSION);
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
update_bps_summary(uint32_t bps)
{
	int i;

	if (bps_summary.used == 0) {
		memset(bps_summary.s, 0, sizeof(struct channelstat));
		bps_summary.s[0].bps = bps;
		bps_summary.used = 1;
		return;
	}

	for (i = 0; i < bps_summary.used; i ++)
		if (bps_summary.s[i].bps == bps) break;

	if (i == bps_summary.used) {
		/* new */
		if (bps_summary.used == bps_summary.size) return;

		memset(bps_summary.s + i, 0, sizeof(struct channelstat));
		bps_summary.s[i].bps = bps;
		bps_summary.used ++;
	}

	return;
}

static void
update_channel_summary(uint32_t channel)
{
	int i;

	if (channel_summary.used == 0) {
		memset(channel_summary.s, 0, sizeof(struct channelstat));
		channel_summary.s[0].channel = channel;
		channel_summary.used = 1;
		return;
	}

	for (i = 0; i < channel_summary.used; i ++)
		if (channel_summary.s[i].channel == channel) break;

	if (i == channel_summary.used) {
		/* new */
		if (channel_summary.used == channel_summary.size) return;

		memset(channel_summary.s + i, 0, sizeof(struct channelstat));
		channel_summary.s[i].channel = channel;
		channel_summary.used ++;
	}

	return;
}

static struct channelstat *
find_channel(uint32_t channel, uint32_t bps)
{
	int i;

	if (stream_stats.used == 0) {
		memset(stream_stats.s, 0, sizeof(struct channelstat));
		stream_stats.s[0].channel = channel;
		stream_stats.s[0].bps = bps;
		stream_stats.used = 1;
		return stream_stats.s;
	}

	for (i = 0; i < stream_stats.used; i ++)
		if (stream_stats.s[i].channel == channel && stream_stats.s[i].bps == bps) break;

	if (i == stream_stats.used) {
		/* new */
		if (stream_stats.used == stream_stats.size) return NULL;
		memset(stream_stats.s + i, 0, sizeof(struct channelstat));
		stream_stats.s[i].channel = channel;
		stream_stats.s[i].bps = bps;
		stream_stats.used ++;
	}

	return stream_stats.s + i;
}

static void
update_global_stats(uint32_t bps, int is_new)
{
	if (bps > minimum_bps) {
		if (is_new) {
			/* new user */
			++ global_stats.visited_users;
			++ global_stats.current_online;
			if (global_stats.current_online > global_stats.max_online) global_stats.max_online = global_stats.current_online;
		} else {
			/* user leave */
			if (global_stats.current_online > 0) -- global_stats.current_online;
		}
	}
}

static void
update_client(uint32_t ip, uint32_t port, uint32_t channel, uint32_t bps, uint32_t ts, uint64_t sid, uint32_t cmd)
{
	uint32_t key;
	struct livestat *st = NULL, *nst = NULL;
	int idx = -1, new_user = 0, r = 0;
	struct sockaddr_in srv;
	struct channelstat *cs;
	struct binary_node *n;

	key = ip;
	idx = (key & hashmask);

	n = binarytree_search(hashmap[idx], key);
	if (n == NULL || n->ptr == NULL) {
		/* new item */
		st = (struct livestat *)calloc(sizeof(struct livestat), 1);
		if (st == NULL) return;

		if (n == NULL) {
			hashmap[idx] = binarytree_insert(hashmap[idx], key, (void *)st, &r);
			if (r) {
				free(st);
				return;
			}
		} else {
			n->ptr = st;
		}

		st->ip = ip; st->port = port;
		st->next = NULL;
		st->ts = cur_ts + expire_time;
		st->start_ts = cur_ts;
		st->bps = bps;
		st->channel = channel;
		st->sid = sid;
		st->last_cmd = cmd;

		if (verbose_mode) {
			memset(&srv, 0, sizeof(srv));
			memcpy(&(srv.sin_addr.s_addr), &ip, sizeof(ip));
			fprintf(stderr, "%s: (%s.%d) CHANNEL #%d/%d NEW USER: %s:%d SID: %lu\n",
					cur_ts_str, __FILE__, __LINE__, channel, bps, inet_ntoa(srv.sin_addr), port, sid);
		}

		new_user = 1;

		update_global_stats(bps, 1);
	} else {
		/* old item */
		nst = st = (struct livestat *) n->ptr;
		while (st) {
			if (sid && sid == st->sid && bps == st->bps && channel == st->channel) /* same sid and same bps*/
				break;
			else if (st->ip == ip && st->port == port) /* same ip & same port */
				break;
			nst = st;
			st = st->next;
		}

		if (st == NULL) {
			/* new client */
			st = (struct livestat *)calloc(sizeof(struct livestat), 1);
			if (st == NULL) return;
			st->ip = ip; st->port = port;
			st->bps = bps;
			st->channel = channel;
			st->next = NULL;
			st->start_ts = cur_ts;
			st->sid = sid;

			new_user = 1;
			nst->next = st;
			update_global_stats(bps, 1);
			if (verbose_mode) {
				memset(&srv, 0, sizeof(srv));
				memcpy(&(srv.sin_addr.s_addr), &ip, sizeof(ip));
				fprintf(stderr, "%s: (%s.%d) CHANNEL #%d/%d NEW USER: %s:%d SID: %lu\n",
						cur_ts_str, __FILE__, __LINE__, channel, bps, inet_ntoa(srv.sin_addr), port, sid);
			}
		} else {
			if (channel != st->channel || bps != st->bps) {
				/* should not happen */
				fprintf(stderr, "%s: (%s.%d) CHANNEL #%d/%d DON'T MATCHED WITH SAVED #%d/%d\n",
						cur_ts_str, __FILE__, __LINE__, channel, bps, st->channel, st->bps);
				
				cs = find_channel(st->channel, st->bps);
				if (cs) {
					if (cs->current_online > 0) -- cs->current_online;
					if (st->start_ts > 0 && cur_ts > st->start_ts) {
						++ cs->total_view_users;
						cs->total_view_seconds += cur_ts - st->start_ts;
					}
				}

				st->bps = bps;
				st->channel = channel;
				st->start_ts = cur_ts;
			}

			if (cmd == REPORT_JOIN && st->last_cmd == cmd && (cur_ts - st->start_ts) <= 10) {
				/* continuous JOIN */
				if (verbose_mode) {
					memset(&srv, 0, sizeof(srv));
					memcpy(&(srv.sin_addr.s_addr), &ip, sizeof(ip));
					fprintf(stderr, "%s: (%s.%d) CHANNEL #%d/%d CONTINOUS JOIN USER: %s:%d SID: %lu, PREV TS %lu, NEW TS %lu\n",
							cur_ts_str, __FILE__, __LINE__, channel, bps, inet_ntoa(srv.sin_addr), port,
							sid, st->start_ts, cur_ts);
				}
				st->start_ts = cur_ts;
			}
		}

		st->ts = cur_ts + expire_time;
		st->last_cmd = cmd;
		st->ip = ip; st->port = port;
		st->sid = sid;
	}

	if (new_user == 1) {
		cs = find_channel(channel, bps);
		if (cs) {
			++ cs->current_online;
			++ cs->visited_users;
			if (cs->current_online > cs->max_online)
				cs->max_online = cs->current_online;
		}
	}
}

static void
delete_client(uint32_t ip, uint32_t port, uint32_t channel, uint32_t bps, uint32_t ts, uint64_t sid)
{
	uint32_t key;
	struct livestat *st, *nst;
	int idx = -1;
	struct sockaddr_in srv;
	struct channelstat *cs;
	struct binary_node *n;

	key = ip;
	idx = (key & hashmask);

	n = binarytree_search(hashmap[idx], key);
	if (n == NULL || n->ptr == NULL) return;

	nst = st = (struct livestat *) n->ptr;
	while (st) {
		if (sid && sid == st->sid && bps == st->bps && channel == st->channel) /* same sid and same bps*/
			break;
		else if (st->ip == ip && st->port == port) /* same ip and same port */
			break;
		nst = st;
		st = st->next;
	}

	if (st == NULL) /* not found */
		return;

	if (st == nst) { /* first item */
		if (st->next == NULL) { /* only 1 item */
			binarytree_delete(hashmap[idx], key);
		} else {
			n->ptr = st->next;
		}
	} else {
		nst->next = st->next;
	}

	update_global_stats(st->bps, 0);

	cs = find_channel(channel, bps);
	if (cs) {
		if (cs->current_online > 0) -- cs->current_online;
		if (st->start_ts > 0 && cur_ts > st->start_ts) {
			++ cs->total_view_users;
			cs->total_view_seconds += cur_ts - st->start_ts;
		}
	}

	if (st->start_ts > 0 && cur_ts > st->start_ts && bps > minimum_bps) {
		++ global_stats.total_view_users;
		global_stats.total_view_seconds += cur_ts - st->start_ts; /* update total time of live system */
	}

	free(st);

	if (verbose_mode) {
		memset(&srv, 0, sizeof(srv));
		memcpy(&(srv.sin_addr.s_addr), &ip, sizeof(ip));
		fprintf(stderr, "%s: (%s.%d) CHANNEL #%d/%d LEAVE USER: %s:%d SID: %lu\n",
				cur_ts_str, __FILE__, __LINE__, channel, bps, inet_ntoa(srv.sin_addr), port, sid);
	}
}

#define BUFLEN 8192

static void
logger_handler(const int fd, const short which, void *arg)
{
	struct sockaddr_in server;
	char buf[BUFLEN+1];
	int r;
	socklen_t len;
	struct report rep;

	len = sizeof(server);
	r = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *) &server, &len);
	if (r == -1 || r != sizeof(rep)) return;

	memcpy(&rep, buf, r);
	if (rep.code != REPORT_MAGIC_CODE) return;

	switch(rep.action) {
		case REPORT_JOIN:
		case REPORT_KEEPALIVE:
			update_client(rep.ip, rep.port, rep.channel, rep.bps, rep.ts, rep.sid, rep.action);
			break;
		case REPORT_LEAVE:
			delete_client(rep.ip, rep.port, rep.channel, rep.bps, rep.ts, rep.sid);
			break;
	}
}

static void
reset_global_stats(void)
{
	struct binary_tree *root;
	struct binary_node *n;
	struct livestat *st, *nst;
	int i, j;

	for (i = 0; i < hashsize; i ++) {
		if (hashmap[i] == NULL) continue;
		root = hashmap[i];
		hashmap[i] = NULL;
		for (j = 0; j < root->used; j ++) {
			n = root->tree[j];
			if (n) {
				st = (struct livestat *)n->ptr;
				while (st) {
					nst = st->next;
					free(st);
					st = nst;
				}
				free(n);
			}
		}
		free(root->tree);
		free(root);
	}

	visited_ts = cur_ts;
	/* reset global stats */
	memset(&global_stats, 0, sizeof(global_stats));

	stream_stats.used = bps_summary.used = channel_summary.used = 0;
	memset(stream_stats.s, 0, sizeof(struct channelstat) * stream_stats.size);
	memset(bps_summary.s, 0, sizeof(struct channelstat) * bps_summary.size);
	memset(channel_summary.s, 0, sizeof(struct channelstat) * channel_summary.size);
}

/* return 0 if added, return 1 if bad arguments */
static int
parse_reporter_timer(char *args)
{
	char *p, *q, *s;
	struct timer *t;

	if (args == NULL || args[0] == '\0') return 1;

	/* time type */
	p = args;
	q = strchr(p, ' ');

	t = (struct timer *) calloc(sizeof(struct timer), 1);
	if (t == NULL) return 1;

	/* type */
	if (q) {
		*q = '\0';
		t->repeat = atoi(q + 1);
	} else {
		t->repeat = 1;
	}

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

	if (q) *q = ' ';

	t->next = NULL;
	if (reset_header == NULL) {
		reset_header = t;
	} else {
		reset_tail->next = t;
	}
	reset_tail = t;
	return 0;
}

static void
admin_handler(const int fd, const short which, void *arg)
{
	struct sockaddr_in server;
	char buf[BUFLEN+1], buf2[BUFLEN+1], buf3[128];
	int r, i;
	socklen_t len;
	double multi;

	len = sizeof(server);
	r = recvfrom(fd, &buf, BUFLEN, 0, (struct sockaddr *) &server, &len);
	if (r == -1) return;

	buf[r] = '\0';
	if (buf[r-1] == '\n') buf[--r] ='\0';

	if (strncasecmp(buf, "help", 4) == 0) {
		r = snprintf(buf2, BUFLEN, "help status report resetall setminbps setmultiply timer cleartimer users\n");
		sendto(fd, buf2, r, 0, (struct sockaddr *)&server, len);
	} else if (strncasecmp(buf, "setmultiply ", 12) == 0) {
		multi = strtod(buf + 12, NULL);
		if (errno == 0) {
			global_multiply = multi;
			r = snprintf(buf2, BUFLEN, "set multiplier to %.2f\n", global_multiply);
			sendto(fd, buf2, r, 0, (struct sockaddr *)&server, len);
		}
	} else if (strncasecmp(buf, "setminbps ", 10) == 0) {
		i = atoi(buf + 10);
		if (i >= 0) {
			minimum_bps = i;
			r = snprintf(buf2, BUFLEN, "set minimum bps to %d kbps\n", minimum_bps);
			sendto(fd, buf2, r, 0, (struct sockaddr *)&server, len);
		}
	} else if (strncasecmp(buf, "resetall", 8) == 0) {
		reset_global_stats();
		r = snprintf(buf2, BUFLEN, "RESETALL OK!\n");
		sendto(fd, buf2, r, 0, (struct sockaddr *)&server, len);
		goto reporter_status;
	} else if (strncasecmp(buf, "stat", 4) == 0) {
		struct timer *t;
reporter_status:
		r = snprintf(buf2, BUFLEN, "%s: MIN BPS #%d, Multiplier %.2f\n",
			cur_ts_str, minimum_bps, global_multiply);
		sendto(fd, buf2, r, 0, (struct sockaddr *)&server, len);

		strftime(buf3, 127, "%Y-%m-%d %H:%M:%S", localtime(&visited_ts));

		r = snprintf(buf2, BUFLEN, "Total %d/#%d Online/Max Users; %d Historical Users since %s\n",
				global_stats.current_online, global_stats.max_online, global_stats.visited_users, buf3);
		sendto(fd, buf2, r, 0, (struct sockaddr *)&server, len);

		r = snprintf(buf2, BUFLEN, "Total View Seconds: %ld, Total View Users: %d, Average Seconds Per User: %.2f\n",
				global_stats.total_view_seconds, global_stats.total_view_users,
				1.0 * global_stats.total_view_seconds / (global_stats.total_view_users?global_stats.total_view_users:1)
				);
		sendto(fd, buf2, r, 0, (struct sockaddr *)&server, len);

		for (i = 0; i < stream_stats.used; i ++) {
			r = snprintf(buf2, BUFLEN, "#%d-%dK \t-> %d/#%d online/max, #%d historical users, seconds %ld/%d/%.2f\n",
					stream_stats.s[i].channel, stream_stats.s[i].bps,
					stream_stats.s[i].current_online, stream_stats.s[i].max_online, stream_stats.s[i].visited_users,
					stream_stats.s[i].total_view_seconds, stream_stats.s[i].total_view_users,
					1.0 * stream_stats.s[i].total_view_seconds / (stream_stats.s[i].total_view_users?stream_stats.s[i].total_view_users:1)
					);
			sendto(fd, buf2, r, 0, (struct sockaddr *)&server, len);
		}

		for (i = 0; i < bps_summary.used; i ++) {
			r = snprintf(buf2, BUFLEN, "#%d KBPS  \t-> %d/#%d online/max, #%d historical users, seconds %ld/%d/%.2f\n",
					bps_summary.s[i].bps, bps_summary.s[i].current_online,
					bps_summary.s[i].max_online, bps_summary.s[i].visited_users,
					bps_summary.s[i].total_view_seconds, bps_summary.s[i].total_view_users,
					1.0 * bps_summary.s[i].total_view_seconds / (bps_summary.s[i].total_view_users?bps_summary.s[i].total_view_users:1)
				    );
			sendto(fd, buf2, r, 0, (struct sockaddr *)&server, len);
		}

		for (i = 0; i < channel_summary.used; i ++) {
			r = snprintf(buf2, BUFLEN, "#%d CHANNEL  \t-> %d/#%d online/max, #%d historical users, seconds %ld/%d/%.2f\n",
					channel_summary.s[i].channel, channel_summary.s[i].current_online,
					channel_summary.s[i].max_online, channel_summary.s[i].visited_users,
					channel_summary.s[i].total_view_seconds, channel_summary.s[i].total_view_users,
					1.0 * channel_summary.s[i].total_view_seconds / (channel_summary.s[i].total_view_users?channel_summary.s[i].total_view_users:1)
				    );
			sendto(fd, buf2, r, 0, (struct sockaddr *)&server, len);
		}

		t = reset_header;
		i = 1;
		while (t) {
			r = snprintf(buf2, BUFLEN, "#%d RESETALL TIMER -> %02d:%02d, %s\n",
					i, t->ts/3600, (t->ts%3600)/60, t->repeat?"Repeat":"Once");
			sendto(fd, buf2, r, 0, (struct sockaddr *)&server, len);
			i ++;
			t = t->next;
		}
	} else if (strncasecmp(buf, "cleartimer", 10) == 0) {
		struct timer *t;

		if (reset_header == NULL) {
			r = snprintf(buf2, BUFLEN, "NO TIMER\n");
		} else {
			while (reset_header) {
				t = reset_header->next;
				free(reset_header);
				reset_header = t;
			}

			reset_tail = reset_header;
			r = snprintf(buf2, BUFLEN, "TIMER CLEARED\n");
		}
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "timer ", 6) == 0) {
		if (parse_reporter_timer(buf + 6) == 0) {
			r = snprintf(buf2, BUFLEN, "Reset Timer %s -> OK\n", buf + 6);
		} else {
			r = snprintf(buf2, BUFLEN, "TRY: TIME [0|1]\n");
		}
		sendto(fd, buf2, r, 0, (struct sockaddr *)&server, len);
	} else if (strncasecmp(buf, "report", 6) == 0) {
		snprintf(buf2, BUFLEN, "<report>\n\t<total max=\"%d\">%d</total>\n",
				(int) (global_stats.max_online * global_multiply),
				(int) (global_stats.current_online * global_multiply)
				);

		snprintf(buf3, 127, "\t<historical start=\"%u\" current=\"%u\">%d</historical>\n", (uint32_t) visited_ts, (uint32_t) cur_ts,
				(int) (global_stats.visited_users * global_multiply));
		strcat(buf2, buf3);

		snprintf(buf3, 127, "\t<average_time total_seconds=\"%ld\" user=\"%d\">%.2f</average_time>\n",
				global_stats.total_view_seconds, global_stats.total_view_users,
				1.0 * global_stats.total_view_seconds / (global_stats.total_view_users?global_stats.total_view_users:1));
		strcat(buf2, buf3);

		strcat(buf2, "\n");
		for (i = 0; i < stream_stats.used; i ++) {
			snprintf(buf3, 127, "\t<channel id=\"%d\" bps=\"%d\" users=\"%d\" max=\"%d\" historical=\"%d\" seconds=\"%ld/%d/%.2f\"/>\n",
					stream_stats.s[i].channel, stream_stats.s[i].bps, stream_stats.s[i].current_online,
					stream_stats.s[i].max_online, stream_stats.s[i].visited_users,
					stream_stats.s[i].total_view_seconds, stream_stats.s[i].total_view_users,
					1.0 * stream_stats.s[i].total_view_seconds / (stream_stats.s[i].total_view_users?stream_stats.s[i].total_view_users:1)
					);
			strcat(buf2, buf3);
		}

		strcat(buf2, "\n");
		for (i = 0; i < bps_summary.used; i ++) {
			snprintf(buf3, 127, "\t<bps-summary bps=\"%d\" users=\"%d\" max=\"%d\" historical=\"%d\" seconds=\"%ld/%d/%.2f\"/>\n",
					bps_summary.s[i].bps, bps_summary.s[i].current_online,
					bps_summary.s[i].max_online, bps_summary.s[i].visited_users,
					bps_summary.s[i].total_view_seconds, bps_summary.s[i].total_view_users,
					1.0 * bps_summary.s[i].total_view_seconds / (bps_summary.s[i].total_view_users?bps_summary.s[i].total_view_users:1)
				);
			strcat(buf2, buf3);
		}

		strcat(buf2, "\n");
		for (i = 0; i < channel_summary.used; i ++) {
			snprintf(buf3, 127, "\t<channel-summary id=\"%d\" users=\"%d\" max=\"%d\" historical=\"%d\" seconds=\"%ld/%d/%.2f\"/>\n",
					channel_summary.s[i].channel, channel_summary.s[i].current_online,
					channel_summary.s[i].max_online, channel_summary.s[i].visited_users,
					channel_summary.s[i].total_view_seconds, channel_summary.s[i].total_view_users,
					1.0 * channel_summary.s[i].total_view_seconds / (channel_summary.s[i].total_view_users?channel_summary.s[i].total_view_users:1)
				);
			strcat(buf2, buf3);
		}

		strcat(buf2, "</report>\n");
		sendto(fd, buf2, strlen(buf2), 0, (struct sockaddr *)&server, len);
	} else if (strncasecmp(buf, "users ", 6) == 0) {
		int start = 0, end = 0;
		char *p, *q;

		p = buf + 6;
		q = strchr(p, ' ');
		if (q) { /* found ' ' */
			*q = '\0'; /* chop p string */
			start = atoi(p);
			end = atoi(q+1);
			*q = ' '; /* restore ' ' */
		} else {
			start = atoi(p);
			end = 0;
		}
		
		r = 0;

		for (i = 0; i < channel_summary.used; i ++) {
			if (end == 0 && channel_summary.s[i].channel == start) {
				r = channel_summary.s[i].current_online;
				break;
			} else if (channel_summary.s[i].channel >= start && channel_summary.s[i].channel <= end) {
				r += channel_summary.s[i].current_online;
			}
		}
		
		snprintf(buf2, BUFLEN, "%d %d\n", start, r == 0?-1: (int)(r*global_multiply));
		sendto(fd, buf2, strlen(buf2), 0, (struct sockaddr *)&server, len);
	} else {
		sendto(fd, NULL, 0, 0, (struct sockaddr *)&server, len);
	}
}

static void
check_binarytree(struct binary_tree *root)
{
	struct livestat *st, *nst, *pst, *first_st = NULL;
	struct channelstat *cs;
	struct binary_node *n;
	int i;

	if (root == NULL) return;
	for (i = 0; i < root->used; i ++) {
		n = root->tree[i];
		if (n) {
			st = (struct livestat *)n->ptr;
			first_st = nst = pst = NULL;
			while (st) {
				nst = st->next;
				if (st->ts <= cur_ts) { /* expired items */
					update_global_stats(st->bps, 0);

					cs = find_channel(st->channel, st->bps);
					if (cs) {
						if (cs->current_online > 0) -- cs->current_online;
						if (st->start_ts > 0 && cur_ts > st->start_ts) {
							++ cs->total_view_users;
							cs->total_view_seconds += cur_ts - st->start_ts;
						}
					}
					
					if (st->start_ts > 0 && cur_ts > st->start_ts && st->bps > minimum_bps) {
						++ global_stats.total_view_users;
						global_stats.total_view_seconds += cur_ts - st->start_ts; /* update total time of live system */
					}

					free(st);
				} else {
					if (pst == NULL) {
						first_st = pst = st;
					} else {
						pst->next = st;
						pst = st;
					}
					st->next = NULL;
				}
				st = nst;
			}
			n->ptr = first_st;
		}
	}
}

static void
cleanup_stats(void)
{
	int i, j, k;

	i = clean_start;
	j = clean_start + clean_step;
	if (j < hashsize) {
		clean_start = j;
	} else {
		j = hashsize;
		clean_start = 0;
	}

	for (k = i; k < j; k ++) {
		check_binarytree(hashmap[k]);
	}
}

static void
reporter_timer_service(const int fd, short which, void *arg)
{
	struct timeval tv;
	struct channelstat *cs = NULL;
	int i, j, channel, bps, day_diff;
	struct tm tm;
	struct timer *t, *tp;

	cur_ts = time(NULL);
	strftime(cur_ts_str, 127, "%Y-%m-%d %H:%M:%S", localtime(&cur_ts));

	if (clean_ts <= cur_ts) {
		cleanup_stats();
		clean_ts = cur_ts + 15; /* cleaning expired users every 15 seconds */
	}

	for (i = 0; i < stream_stats.used; i ++) {
		update_bps_summary(stream_stats.s[i].bps);
		update_channel_summary(stream_stats.s[i].channel);
	}

	for (j = 0; j < bps_summary.used; j ++) {
		cs = bps_summary.s + j;
		bps = cs->bps;

		cs->visited_users = 0;
		cs->current_online = 0;
		cs->total_view_users = 0;
		cs->total_view_seconds = 0;
		for (i = 0; i < stream_stats.used; i ++) {
			if (stream_stats.s[i].bps == bps) {
				cs->visited_users += stream_stats.s[i].visited_users;
				cs->current_online += stream_stats.s[i].current_online;
				cs->total_view_users += stream_stats.s[i].total_view_users;
				cs->total_view_seconds += stream_stats.s[i].total_view_seconds;
			}
		}

		if (cs->current_online > cs->max_online) cs->max_online = cs->current_online;
	}

	for (j = 0; j < channel_summary.used; j ++) {
		cs = channel_summary.s + j;
		channel = cs->channel;

		cs->visited_users = 0;
		cs->current_online = 0;
		cs->total_view_users = 0;
		cs->total_view_seconds = 0;
		for (i = 0; i < stream_stats.used; i ++) {
			if (stream_stats.s[i].channel == channel) {
				cs->visited_users += stream_stats.s[i].visited_users;
				cs->current_online += stream_stats.s[i].current_online;
				cs->total_view_users += stream_stats.s[i].total_view_users;
				cs->total_view_seconds += stream_stats.s[i].total_view_seconds;
			}
		}

		if (cs->current_online > cs->max_online) cs->max_online = cs->current_online;
	}

	if ((cur_ts % 60) == 0) {
		memset(&tm, 0, sizeof(tm));
		localtime_r(&cur_ts, &tm);
		day_diff = tm.tm_min * 60 + tm.tm_hour * 3600;

		t = tp = reset_header;
		while (t) {
			if (day_diff != t->ts) {
				/* SKIP */
				tp = t; t = t->next;
				continue;
			}
			/* reset status */
			reset_global_stats();

			if (t->repeat == 0) {
				/* remove it from list */
				if (t == reset_header) {
					reset_header = t->next;
					if (reset_header == NULL) reset_tail = NULL;
					tp = t = reset_header;
				} else {
					tp->next = t->next;
					free(t);
					t = tp->next;
				}
			} else {
				tp = t; t = t->next;
			}
		}
	}

	tv.tv_sec = 1; tv.tv_usec = 0;
	evtimer_set(&ev_timer, reporter_timer_service, NULL);
	event_add(&ev_timer, &tv);
}

int
main(int argc, char **argv)
{
	int c, to_daemon = 1, udpfd, port = 5600;
	int admin_fd, admin_port = 5700, maxchannels = 1024;
	struct sockaddr_in udpaddr;
	struct event ev_udp, ev_admin;
	char *bindhost = NULL;
	struct timeval tv;

	while (-1 != (c = getopt(argc, argv, "hDp:l:m:M:t:vC:R:"))) {
		switch(c)  {
		case 'D':
			to_daemon = 0;
			break;
		case 'v':
			verbose_mode = 1;
			break;
		case 'C':
			maxchannels = atoi(optarg);
			if (maxchannels <=0 ) maxchannels = 4096;
			break;
		case 'm':
			admin_port = atoi(optarg);
			if (admin_port <= 0) admin_port = 5700;
			break;
		case 'M':
			minimum_bps = atoi(optarg);
			if (minimum_bps < 0) minimum_bps = 0;
			break;
		case 'p':
			port = atoi(optarg);
			if (port <= 0) port = 5600;
			break;
		case 'l':
			bindhost = optarg;
			break;
		case 't':
			expire_time = atoi(optarg);
			if (expire_time <= 0) expire_time = 240; /* 4 minutes */
			break;
		case 'h':
		default:
			show_reporter_help();
			return 1;
		}
	}

	hashsize = 1024 * 1024;
	hashmask = hashsize - 1;

	hashmap = (struct binary_tree **)calloc(sizeof(struct binary_tree *), hashsize);

	if (hashmap == NULL) {
		fprintf(stderr, "(%s.%d) not enough memory for hashmap\n", __FILE__, __LINE__);
		return 1;
	}

	/* reset global stats */
	memset(&global_stats, 0, sizeof(global_stats));

	memset((char *) &udpaddr, 0, sizeof(udpaddr));
	udpaddr.sin_family = AF_INET;
	if (bindhost == NULL || bindhost[0] == '\0')
		udpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		udpaddr.sin_addr.s_addr = inet_addr(bindhost);

	udpaddr.sin_port = htons(port);

	udpfd = socket(AF_INET, SOCK_DGRAM, 0);
	admin_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (udpfd < 0 || admin_fd < 0) {
		fprintf(stderr, "(%s.%d) can't create udp socket\n", __FILE__, __LINE__);
		free(hashmap);
		close(udpfd);
		close(admin_fd);
		return 1;
	}

	/* logger udp */
	set_nonblock(udpfd);
	if (bind(udpfd, (struct sockaddr *) &udpaddr, sizeof(udpaddr))) {
		if (errno != EINTR)
			fprintf(stderr, "(%s.%d) errno = %d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		close(udpfd);
		free(hashmap);
		return 1;
	}

	/* admin udp */
	memset((char *) &udpaddr, 0, sizeof(udpaddr));
	udpaddr.sin_family = AF_INET;
	if (bindhost == NULL || bindhost[0] == '\0')
		udpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		udpaddr.sin_addr.s_addr = inet_addr(bindhost);

	udpaddr.sin_port = htons(admin_port);
	set_nonblock(admin_fd);
	if (bind(admin_fd, (struct sockaddr *) &udpaddr, sizeof(udpaddr))) {
		if (errno != EINTR)
			fprintf(stderr, "(%s.%d) errno = %d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		close(udpfd);
		close(admin_fd);
		free(hashmap);
		return 1;
	}

	/* init channel summarys */
	stream_stats.s = (struct channelstat *) calloc(sizeof(struct channelstat), maxchannels);
	bps_summary.s = (struct channelstat *) calloc(sizeof(struct channelstat), maxchannels);
	channel_summary.s = (struct channelstat *) calloc(sizeof(struct channelstat), maxchannels);

	if (stream_stats.s == NULL || bps_summary.s == NULL || channel_summary.s == NULL) {
		fprintf(stderr, "out of memory\n");
		return 1;
	}

	stream_stats.used = bps_summary.used = channel_summary.used = 0;
	stream_stats.size = bps_summary.size = channel_summary.size = maxchannels;

	visited_ts  = cur_ts = time(NULL);
	clean_ts = cur_ts + expire_time;
	strftime(cur_ts_str, 127, "%Y-%m-%d %H:%M:%S", localtime(&cur_ts));

	fprintf(stderr, "%s: Youku Live Reporter v%s(" __DATE__ " " __TIME__ "), LOGGER: UDP 0.0.0.0:%d, ADMIN: UDP 0.0.0.0:%d\n",
			cur_ts_str, VERSION, port, admin_port);

	if (to_daemon && daemonize(0, 0) == -1) {
		fprintf(stderr, "(%s.%d) failed to be a daemon\n", __FILE__, __LINE__);
		exit(1);
	}

	signal(SIGPIPE, SIG_IGN);

	clean_step = hashsize / 4;
	clean_start = 0;

	event_init();
	event_set(&ev_udp, udpfd, EV_READ|EV_PERSIST, logger_handler, NULL);
	event_add(&ev_udp, 0);

	event_set(&ev_admin, admin_fd, EV_READ|EV_PERSIST, admin_handler, NULL);
	event_add(&ev_admin, 0);

	tv.tv_sec = 1; tv.tv_usec = 0;
	evtimer_set(&ev_timer, reporter_timer_service, NULL);
	event_add(&ev_timer, &tv);

	event_dispatch();
	return 0;
}
