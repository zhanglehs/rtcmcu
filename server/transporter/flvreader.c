/* small http server which read local flv files and transfer them to live stream
 * Que Hongyu, 2009-08-25
 */
#define _GNU_SOURCE
#include <sys/types.h>
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
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <event.h>
#include <errno.h>
#include <time.h>
#include <sys/uio.h>
#include <signal.h>

#include "xml.h"
#include "buffer.h"

#define SPEED_TS 1

/* client reader_connection structure */
struct reader_connection
{
	int fd;
	int state;
	int http_status;
	struct event ev;
	int idx;

	buffer *r, *w;
	int wpos;
	int live_start;

	uint64_t start_ts;

	buffer *flv;
	char *flvfile;

	time_t flv_file_ts;

	buffer *onmetadata;
	buffer *avc0;
	buffer *aac0;

	int width, height, framerate;

	time_t video_time;
	uint32_t video_start, video_end;

	uint32_t speed_ts;
	uint32_t bytes_out;
	uint32_t throttle_ts;

	uint32_t maxspeed;
	int is_writable;

	time_t pull_start_ts, pull_time;
};

typedef struct reader_connection conn;

struct urlmap
{
	char *url;
	char *file;
	char *backup;

	int use_backup;

	time_t flv_file_ts;

	buffer *content;
};

struct timer
{
	int ts; /* second of day since 0:0:00 */
	int action; /* 0 -> use backup, 1 -> use main */
	char *url;

	int repeat;

	struct timer *next;
};

static struct timer *timer_header = NULL, *timer_tail = NULL;

struct reader_config
{
	int port;
	int udp_port;

	char bindhost[33];
	int maxconns;

	int map_count;
	struct urlmap *maps;
};

static struct reader_config conf;
static conn *client_conns;
static int verbose_mode = 0, use_seekable = 0;
static struct event ev_timer;
static time_t cur_ts = 0;
static char cur_ts_str[128];

static void flvreader_http_handler(const int , const short , void *);

static const char resivion[] __attribute__((used)) = { "$Id: flvreader.c 534 2012-04-11 03:58:10Z qhy $" };

static void
show_reader_help(void)
{
	fprintf(stderr, "Youku FLV Reader for Flash Live System v%s, Build-Date: " __DATE__ " " __TIME__ "\n"
		  "Usage:\n"
		  " -h this message\n"
		  " -c file, config file, default is /etc/flash/flvreader.xml\n"
		  " -D don't go to background\n"
		  " -s loop at seekable tag\n"
		  " -v verbose\n\n", VERSION);
}

static void
flvreader_conn_reset(conn *c)
{
	if (c == NULL) return;

	if (c->onmetadata) c->onmetadata->used = 0;
	if (c->avc0) c->avc0->used = 0;
	if (c->aac0) c->aac0->used = 0;
	if (c->r) c->r->used = 0;
	if (c->w) c->w->used = 0;
	if (c->flv) c->flv->used = 0;

	c->framerate = 0;
	c->width = c->height = c->start_ts = 0;
	c->state = c->http_status = 0;
	c->idx = -1;
	c->wpos = c->live_start = 0;
	c->flv_file_ts = 0;
	c->video_time = c->video_start = c->video_end = 0;
	c->throttle_ts = c->bytes_out = c->speed_ts = c->maxspeed = 0;

	c->pull_time = 0;
	c->pull_start_ts = 0;
}

static void
flvreader_conn_close(conn *c)
{
	if (c == NULL) return;
	if (c->fd > 0) {
		if (verbose_mode)
			fprintf(stderr, "%s: (%s.%d) close read fd #%d\n", cur_ts_str, __FILE__, __LINE__, c->fd);
		event_del(&(c->ev));
		close(c->fd);
		c->fd = 0;
	}
	flvreader_conn_reset(c);
}

static void
parse_flv_onmetadata(conn *c, int start, int size)
{
	int pos;
	uint32_t value = 0;

	if (c == NULL || c->flv == NULL || c->flv->ptr == NULL) return;

	c->width = c->height = c->framerate = 0;

	pos = memstr(c->flv->ptr + start, "onMetaData", size, 10 /*strlen("onMetaData") */);
	if (pos < 0) return; /* not onMetaData tag */

	/* to find "height" */
	pos = memstr(c->flv->ptr + start, "height", size, strlen("height"));
	if (pos >= 0) {
		value = 0;
		/* we got it */
		pos += strlen("height");
		switch(c->flv->ptr[pos + start]) {
			case 0: /* double */
				value = (uint32_t) get_double((unsigned char *)(c->flv->ptr + pos + start + 1));
				break;
			default:
				break;
		}
		if (value > 0)
			c->height = value;
	}

	/* to find "width" */
	pos = memstr(c->flv->ptr + start, "width", size, strlen("width"));
	if (pos >= 0) {
		value = 0;
		/* we got it */
		pos += strlen("width");
		switch(c->flv->ptr[pos + start]) {
			case 0: /* double */
				value = (uint32_t) get_double((unsigned char *)(c->flv->ptr + pos + start + 1));
				break;
			default:
				break;
		}
		if (value > 0)
			c->width = value;
	}

	/* to find "framerate" */
	pos = memstr(c->flv->ptr + start, "framerate", size, strlen("framerate"));
	if (pos >= 0) {
		value = 0;
		/* we got it */
		pos += strlen("framerate");
		switch(c->flv->ptr[pos + start]) {
			case 0: /* double */
				value = (uint32_t) get_double((unsigned char *)(c->flv->ptr + pos + start + 1));
				break;
			default:
				break;
		}
		if (value > 0)
			c->framerate = value;
	}
}

static void
generate_flv_onmetadata(conn *c)
{
	int len = 3;
	FLVTag tag;
	char *dst;
	buffer *b;

	if (c == NULL) return;
	if (c->onmetadata == NULL) c->onmetadata = buffer_init(256);
	if (c->onmetadata == NULL) return;

	b = c->onmetadata;
	if (c->width > 0) len ++;
	if (c->height > 0) len ++;
	if (c->framerate > 0) len ++;

	memset(&tag, 0, sizeof(tag));
	tag.type = FLV_SCRIPTDATAOBJECT;
	b->used = sizeof(tag);

	append_script_dataobject(b, "onMetaData", len); /* variables */

	append_script_datastr(b, "starttime", strlen("starttime"));
	append_script_var_double(b, (double) c->start_ts);
	append_script_datastr(b, "hasVideo", strlen("hasVideo"));
	append_script_var_bool(b, 1);
	append_script_datastr(b, "hasAudio", strlen("hasAudio"));
	append_script_var_bool(b, 1);

	if (c->width > 0) {
		append_script_datastr(b, "width", strlen("width"));
		append_script_var_double(b, (double) c->width);
	}

	if (c->height > 0) {
		append_script_datastr(b, "height", strlen("height"));
		append_script_var_double(b, (double) c->height);
	}

	if (c->framerate > 0) {
		append_script_datastr(b, "framerate", strlen("framerate"));
		append_script_var_double(b, (double) c->framerate);
	}

	append_script_emca_array_end(b);

	len = b->used - sizeof(tag);
	/* update onmetadata tag structure */
	tag.datasize[0] = (len >> 16) & 0xff;
	tag.datasize[1] = (len >> 8) & 0xff;
	tag.datasize[2] = len & 0xff;

	memcpy(b->ptr, &tag, sizeof(tag));

	/* append 4B of PREV_TAGSIZE */
	dst = b->ptr + b->used;
	dst[0] = (b->used >> 24) & 0xff;
	dst[1] = (b->used >> 16) & 0xff;
	dst[2] = (b->used >> 8) & 0xff;
	dst[3] = b->used & 0xff;
	b->used += 4;
}

/* reset tag's timestamp starting from zero */
static void
init_flv_timestamp(conn *c, time_t first_ts)
{
	FLVTag *flvtag;
	int pos, tagsize, datasize;
	uint32_t ts;

	if (c == NULL || c->flv == NULL) return;
	pos = c->video_start;
	while(pos < c->video_end) {
		flvtag = (FLVTag *) (c->flv->ptr + pos);
		tagsize = (flvtag->datasize[0] << 16) + (flvtag->datasize[1] << 8) + (flvtag->datasize[2]);
		datasize = tagsize + sizeof(FLVTag) + 4;

		if ((pos + datasize )> c->flv->used) break;

		ts = (flvtag->timestamp[0] << 16) + (flvtag->timestamp[1] << 8) + (flvtag->timestamp[2]);
		ts += (flvtag->timestamp_ex << 24);

		if (ts > first_ts) ts -= first_ts;
		else ts = 0;

		flvtag->timestamp_ex = (ts >> 24) & 0xff;
		flvtag->timestamp[0] = (ts >> 16) & 0xff;
		flvtag->timestamp[1] = (ts >> 8) & 0xff;
		flvtag->timestamp[2] = ts & 0xff;
		pos += datasize;
	}
}

static void
remove_non_onmetadata_tag(buffer *dst, buffer *src)
{
	int pos, tagsize, datasize, pos2;
	FLVTag *flvtag;

	if (dst == NULL || src == NULL) return;

	dst->used = pos = 13; /* sizeof(FLVHeader) + 4B */
	memcpy(dst->ptr, src->ptr, pos);
	while (pos < src->used) {
		flvtag = (FLVTag *) (src->ptr + pos);
		tagsize = (flvtag->datasize[0] << 16) + (flvtag->datasize[1] << 8) + (flvtag->datasize[2]);
		datasize = tagsize + sizeof(FLVTag) + 4;

		if ((pos + datasize )> src->used) break;

		if (flvtag->type == FLV_SCRIPTDATAOBJECT) {
			pos2 = memstr(src->ptr + pos + sizeof(FLVTag), "onMetaData",
					tagsize, 10 /*strlen("onMetaData")*/);
			if (pos2 > 0) {
				memcpy(dst->ptr + dst->used, src->ptr + pos, datasize);
				dst->used += datasize;
			}
		} else {
			memcpy(dst->ptr + dst->used, src->ptr + pos, datasize);
			dst->used += datasize;
		}
		pos += datasize;
	}
}

/* return 0 if successed
 * return 1 if failed
 */
static int
parse_flv_file(conn *c)
{
	int pos, tagsize, datasize;
	FLVTag *flvtag;
	int start_pos = 0, start_ts = 0, ts = 0, has_first_ts = 0;
	int end_pos = 0, end_ts = 0;
	char *p;
	struct timeval tv;

	if (c == NULL || c->flv == NULL || c->flv->used <= 13) return 1;

	pos = 13; /* sizeof(FLVHeader) + 4B */
	while(pos < c->flv->used) {
		flvtag = (FLVTag *) (c->flv->ptr + pos);
		tagsize = (flvtag->datasize[0] << 16) + (flvtag->datasize[1] << 8) + (flvtag->datasize[2]);
		datasize = tagsize + sizeof(FLVTag) + 4;

		if ((pos + datasize )> c->flv->used) break;

		p = c->flv->ptr + pos + sizeof(FLVTag);
		ts = (flvtag->timestamp[0] << 16) + (flvtag->timestamp[1] << 8) + (flvtag->timestamp[2]);
		ts += (flvtag->timestamp_ex << 24);
		if (flvtag->type == FLV_SCRIPTDATAOBJECT) {
			start_pos = pos + datasize;
			parse_flv_onmetadata(c, pos, datasize);
		} else if (flvtag->type == FLV_AUDIODATA) {
			if (((p[0] & 0xf0) == 0xA0) && (p[1] == 0)) {
				start_pos = pos + datasize;
				if (c->aac0 == NULL) c->aac0 = buffer_init(datasize + 1);
				else if (c->aac0->size < datasize) buffer_expand(c->aac0, datasize + 1);
				if (c->aac0) {
					memcpy(c->aac0->ptr, c->flv->ptr + pos, datasize);
					c->aac0->used = datasize;
					/* reset aac0 tag timestamp */
					c->aac0->ptr[4] = c->aac0->ptr[5] = c->aac0->ptr[6] = c->aac0->ptr[7] = '\0';
				}
			} else {
				if (has_first_ts == 0) {
					start_ts = ts;
					has_first_ts = 1;
				} else {
					if (start_ts > ts) start_ts = ts;
				}
			}
		} else if (flvtag->type == FLV_VIDEODATA) {
			if (((p[0] & 0xf) == 0x7) && (p[1] == 0)) {
				start_pos = pos + datasize;
				if (c->avc0 == NULL) c->avc0 = buffer_init(datasize + 1);
				else if (c->avc0->size < datasize) buffer_expand(c->avc0, datasize + 1);

				if (c->avc0) {
					memcpy(c->avc0->ptr, c->flv->ptr + pos, datasize);
					c->avc0->used = datasize;
					/* reset avc0 tag timestamp */
					c->avc0->ptr[4] = c->avc0->ptr[5] = c->avc0->ptr[6] = c->avc0->ptr[7] = '\0';
				}
			} else {
				if (has_first_ts == 0) {
					start_ts = ts;
					has_first_ts = 1;
				} else {
					if (start_ts > ts) start_ts = ts;
				}

				if (use_seekable == 0 || ((p[0] & 0xf0) == 0x10) || ((p[0] & 0xf0) == 0x40)) {
					/* seekable or any time*/
					end_pos = pos;
					end_ts = ts;
				}
			}
		}

		pos += datasize;
	}

	c->video_time = end_ts - start_ts;
	c->pull_time = (int) (c->video_time * 1.0/1000 + 0.5);
	c->video_start = start_pos;
	c->video_end = end_pos; /* pos; */

	c->maxspeed = (c->video_end - c->video_start) * SPEED_TS * 1000.0 / c->video_time;

	init_flv_timestamp(c, start_ts);

	/* after every step completed, use newest timestamp */
	gettimeofday(&tv, NULL);
	c->start_ts = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	generate_flv_onmetadata(c);

	if (verbose_mode)
		fprintf(stderr, "%s: (%s.%d) client fd #%d -> '%s', video time length = %.2fs, "
				"pull time = %lds, video start pos -> %d, video end -> %d, maxspeed = %d bps\n",
				cur_ts_str, __FILE__, __LINE__, c->fd,
				c->flvfile, 1.0 * c->video_time/1000.0, c->pull_time,
				c->video_start, c->video_end, c->maxspeed * 8);

	return 0;
}

/* return 0 if successed
 * return 1 if failed
 */
static struct buffer *
load_flv_file(char *file)
{
	int len, size = 0;
	FILE *fp = NULL;
	buffer *b = NULL;

	fp = fopen(file, "rb");
	if (fp == NULL) return NULL;

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	b = buffer_init(size + 1);
	if (b) {
		len = fread(b->ptr, 1, size, fp);
		if (len != size || strncmp(b->ptr, "FLV", 3)) {
			buffer_free(b);
			b = NULL;
		} else {
			b->used = len;
		}
	}
	fclose(fp);
	return b;
}

static void
parse_flvreader_request(conn *c)
{
	char *p;
	int i, len;
	buffer *b;

	if (c == NULL || c->r == NULL) return;

	if (strncmp(c->r->ptr, "GET ", 4) != 0 || c->r->ptr[4] != '/') {
		c->http_status = 400; /* Bad Request */
		return;
	}
	p = c->r->ptr + 4;

	for (i = 0; i < conf.map_count; i ++) {
		len = strlen(conf.maps[i].url);
		/* '/url?' or '/url ' */
		if (strncmp(p, conf.maps[i].url, len) == 0 && (p[len] == ' ' || p[len] == '?'))
			break;
	}

	if (i == conf.map_count || conf.maps[i].content == NULL) {
		c->http_status = 404;
		return;
	}

	c->flvfile = conf.maps[i].file;
	if (c->flv) {
		if (c->flv->size < conf.maps[i].content->used) {
			buffer_expand(c->flv, conf.maps[i].content->used + 1);
		}
	} else {
		c->flv = buffer_init(conf.maps[i].content->used + 1);
	}

	if (c->flv->size < conf.maps[i].content->used) {
		c->http_status = 503;
		return;
	}
	b = buffer_init(c->flv->size);
	if (b == NULL) {
		c->http_status = 503;
		return;
	}
	buffer_copy(b, conf.maps[i].content);

	/* only onmetadata script object remained */
	remove_non_onmetadata_tag(c->flv, b);
	buffer_free(b);
//	buffer_copy(c->flv, conf.maps[i].content);
	c->flv_file_ts = conf.maps[i].flv_file_ts;

	c->idx = i;

	c->speed_ts = 0;
	c->throttle_ts = 0;

	parse_flv_file(c);
	c->w->used = snprintf(c->w->ptr, c->w->size,
		"HTTP/1.0 200 OK\r\n"
		"Content-Type: video/x-flv\r\n"
		"Expires: -1\r\nCache-Control: private, max-age=0\r\nPragma: no-cache\r\n"
		"\r\n");
#define FLVHEADER "FLV\x1\x5\0\0\0\x9\0\0\0\0" /* 9 plus 4bytes of UINT32 0 */
#define FLVHEADER_SIZE 13
	/* we need to fill standard flv header */
	memcpy(c->w->ptr + c->w->used, FLVHEADER, FLVHEADER_SIZE /* sizeof(FLVHEADER) - 1 */);
	c->w->used += FLVHEADER_SIZE;
#undef FLVHEADER
#undef FLVHEADER_SIZE

	if (c->onmetadata) buffer_append(c->w, c->onmetadata);
	if (c->avc0) buffer_append(c->w, c->avc0);
	if (c->aac0) buffer_append(c->w, c->aac0);
	c->pull_start_ts = cur_ts;
}

static void
retag_flv(conn *c)
{
	FLVTag *flvtag;
	int pos, tagsize, datasize;
	uint32_t ts;

	if (c == NULL || c->flv == NULL) return;

	pos = c->video_start;
	while (pos < c->video_end) {
		flvtag = (FLVTag *) (c->flv->ptr + pos);
		tagsize = (flvtag->datasize[0] << 16) + (flvtag->datasize[1] << 8) + (flvtag->datasize[2]);
		datasize = tagsize + sizeof(FLVTag) + 4;

		if ((pos + datasize )> c->flv->used) break;

		ts = (flvtag->timestamp[0] << 16) + (flvtag->timestamp[1] << 8) + (flvtag->timestamp[2]);
		ts += (flvtag->timestamp_ex << 24);
		ts += c->video_time;

		flvtag->timestamp_ex = (ts >> 24) & 0xff;
		flvtag->timestamp[0] = (ts >> 16) & 0xff;
		flvtag->timestamp[1] = (ts >> 8) & 0xff;
		flvtag->timestamp[2] = ts & 0xff;
		pos += datasize;
	}
}

static void
disable_flvreader_write(conn *c)
{
	if (c && c->fd > 0 && c->is_writable == 1) {
		event_del(&c->ev);
		event_set(&c->ev, c->fd, EV_READ|EV_PERSIST, flvreader_http_handler, (void *)c);
		event_add(&c->ev, 0);
		c->is_writable = 0;
	}
}

static void
enable_flvreader_write(conn *c)
{
	if (c && c->fd > 0 && c->is_writable == 0) {
		event_del(&c->ev);
		event_set(&c->ev, c->fd, EV_READ|EV_WRITE|EV_PERSIST, flvreader_http_handler, (void *)c);
		event_add(&c->ev, 0);
		c->is_writable = 1;
	}
}

static void
write_reader_client(conn *c)
{
	int r = 0, cnt = 0, already_throttle = 0;
	struct iovec chunks[2];

	if (c == NULL) return;

	if (c->wpos < c->w->used) {
		chunks[0].iov_base = c->w->ptr + c->wpos;
		chunks[0].iov_len = c->w->used - c->wpos;
		cnt = 1;
	}

	if (c->http_status == 0 && (c->live_start < c->video_end)) {
		chunks[cnt].iov_base = c->flv->ptr + c->live_start;
		r = c->video_end - c->live_start;
#define BUFFERSIZE 1024
		if (r > BUFFERSIZE) r = BUFFERSIZE;
#undef BUFFERSIZE
		chunks[cnt].iov_len = r;
		cnt ++;
	}

	if (cnt == 0) {
		flvreader_conn_close(c);
		return;
	}

	r = writev(c->fd, (struct iovec *)&chunks, cnt);
	if (r < 0) {
	       	if (errno != EINTR && errno != EAGAIN) {
			flvreader_conn_close(c);
			return;
		}
		r = 0;
	}

	/* update speed counter */
	if (c->speed_ts == 0) {
		c->speed_ts = cur_ts + SPEED_TS;
		c->bytes_out = r;
	} else {
		if (c->speed_ts <= cur_ts) {
			c->bytes_out = r;
			c->speed_ts = cur_ts + SPEED_TS;
		} else {
			c->bytes_out += r;
		}
	}

	if (c->w->used > 0 && c->wpos <= c->w->used) {
		if (r >= (c->w->used - c->wpos)) {
			r -= (c->w->used - c->wpos);
			c->wpos = c->w->used = 0;
			if (c->http_status > 0)
				flvreader_conn_close(c);
		} else {
			c->wpos += r;
			r = 0;
		}
	}

	c->live_start += r;
	if (c->live_start == c->video_end) {
		/* loop */
		if (conf.maps[c->idx].flv_file_ts != c->flv_file_ts) {
			/* file changed */
			flvreader_conn_close(c);
			return;
		}

		retag_flv(c);
		c->live_start = c->video_start;

		if (cur_ts >= (c->pull_start_ts + c->pull_time)) {
			c->pull_start_ts = cur_ts;
		} else {
			c->throttle_ts = c->pull_start_ts + c->pull_time;
			c->pull_start_ts = c->throttle_ts;
			disable_flvreader_write(c);
			already_throttle = 1;
		}
	}

	if (already_throttle == 0 && c->maxspeed > 0 && (c->bytes_out >= c->maxspeed)) {
		disable_flvreader_write(c);
		c->throttle_ts = cur_ts + SPEED_TS;
	}
}

static void
flvreader_http_handler(const int fd, const short which, void *arg)
{
	conn *c;
	int r;
	char buf[129];

	c = (conn *)arg;
	if (c == NULL) return;

	if (which & EV_READ) {
		switch(c->state) {
			case 0: /* READ */
				r = read_buffer(fd, c->r);
				if (r < 0) {
					flvreader_conn_close(c);
				} else if (r > 0) {
					/* terminate it with '\0' for following strstr() */
					c->r->ptr[c->r->used] = '\0';

					if (strstr(c->r->ptr, "\r\n\r\n")) {
						/* end of request http header */
						parse_flvreader_request(c);

						if (c->http_status == 0) {
							c->state = 1; /* WRITE */
						} else {
							switch (c->http_status) {
								case 404: /* Not Found */
									c->w->used = snprintf(c->w->ptr, c->w->size, "HTTP/1.0 404 Not Found\r\nConnection: close\r\n\r\n");
									break;
								case 503: /* Service Not Available */
									c->w->used = snprintf(c->w->ptr, c->w->size, "HTTP/1.0 503 Service Not Available\r\nConnection: close\r\n\r\n");
									break;
								case 400: /* Bad Request */
								default:
									c->w->used = snprintf(c->w->ptr, c->w->size, "HTTP/1.0 400 Bad Request\r\nConnection: close\r\n\r\n");
									break;
							}
						}
						c->wpos = 0;
						c->live_start = c->video_start;
						c->throttle_ts = 0;
						c->speed_ts = cur_ts + SPEED_TS;
						c->bytes_out = 0;
						enable_flvreader_write(c);
						write_reader_client(c);
					}
				}
				break;
			case 1: /* write live flash stream */
				/* just drop incoming unused bytes */
				r = read(fd, buf, 128);
				if ((r == -1 && errno != EINTR && errno != EAGAIN) || r == 0)  {
					flvreader_conn_close(c);
				}
				break;
		}
	} else if (which & EV_WRITE) {
		write_reader_client(c);
	}

	return;
}

static void
flvreader_client_accept(const int fd, const short which, void *arg)
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

	if (c->r == NULL) c->r = buffer_init(512);
	if (c->w == NULL) c->w = buffer_init(512);

	if (c->r == NULL || c->w == NULL) {
		write(newfd, "OUT OF MEMORY\n", strlen("OUT OF MEMORY\n"));
		close(newfd);
		return;
	}
	c->fd = newfd;
	set_nonblock(c->fd);

	c->state = 0; /* READ */
	c->idx = -1;

	c->is_writable = 0;
	event_set(&(c->ev), c->fd, EV_READ|EV_PERSIST, flvreader_http_handler, (void *) c);
	event_add(&(c->ev), 0);
}

static void
reader_timer_service(const int fd, short which, void *arg)
{
	struct timeval tv;
	int i, j, day_diff, changed = 0;
	conn *c;
	struct stat st;
	struct timer *t, *tp = NULL;
	struct tm tm;
	char *url;

	cur_ts = time(NULL);
	strftime(cur_ts_str, 127, "%Y-%m-%d %H:%M:%S", localtime(&cur_ts));

	tv.tv_sec = 1; tv.tv_usec = 0; /* check for every 1 seconds */
	event_add(&ev_timer, &tv);

	for(i = 0; i < conf.maxconns; i ++) {
		c = client_conns + i;
		if (c->fd > 0 && c->throttle_ts > 0 && c->throttle_ts <= cur_ts) {
			c->speed_ts = cur_ts + SPEED_TS;
			c->bytes_out = 0;
			c->throttle_ts = 0;
			enable_flvreader_write(c);
			write_reader_client(c);
		}
	}

	if ((cur_ts % 60) == 0) { /* every 60 seconds */
		memset(&tm, 0, sizeof(tm));
		localtime_r(&cur_ts, &tm);
		day_diff = tm.tm_min * 60 + tm.tm_hour * 3600 + tm.tm_sec;

		for (i = 1, t = tp = timer_header; t; ++i) {
			if (day_diff != t->ts) {
				/* SKIP */
				tp = t; t = t->next;
				continue;
			}

			/* matched */
			switch (t->action) {
				case 0: /* USE BACKUP */
					for (j = 0; j < conf.map_count; j ++) {
						if (conf.maps[j].backup && (strcmp(conf.maps[j].url, t->url) == 0)) {
							/* found match */
							conf.maps[j].use_backup = 1;
							break;
						}
					}

					break;
				case 1: /* USE MAIN */
					for (j = 0; j < conf.map_count; j ++) {
						if (strcmp(conf.maps[j].url, t->url) == 0) {
							/* found match */
							conf.maps[j].use_backup = 0;
							break;
						}
					}
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
					free(t->url);
					free(t);
					t = tp->next;
				}
			} else {
				tp = t; t = t->next;
			}
		}
	}

	for (i = 0; i < conf.map_count; i ++) {
		changed = 0;
		memset(&st, 0, sizeof(st));

		if (conf.maps[i].backup && conf.maps[i].use_backup)
			url = conf.maps[i].backup;
		else
			url = conf.maps[i].file;
		
		/* read regular file and symbol link */
		if ((-1 == stat(url, &st)) || !((S_IFREG & st.st_mode) || (S_IFLNK & st.st_mode))) {
			buffer_free(conf.maps[i].content);
			conf.maps[i].content = NULL;
			changed = 1;
		} else {
			if (conf.maps[i].flv_file_ts != st.st_mtime) {
				changed = 1;
				if (verbose_mode)
					fprintf(stderr, "%s: (%s.%d) flv %s changed, reload it!\n", cur_ts_str, __FILE__, __LINE__, url);
				buffer_free(conf.maps[i].content);
				conf.maps[i].content = load_flv_file(url);
				if (conf.maps[i].content)
					conf.maps[i].flv_file_ts = st.st_mtime;
			}
		}

		if (changed == 1) {
			for(j = 0; j < conf.maxconns;j ++) {
				c = client_conns + j;
				if (c->fd > 0 && c->idx == i) {
					/* file changed */
					flvreader_conn_close(c);
				}
			}
		}
	}
}

static void
init_flvfiles(void)
{
	int i;
	struct stat st;

	for (i = 0; i < conf.map_count; i ++) {
		conf.maps[i].flv_file_ts = 0;
		conf.maps[i].content = NULL;
		memset(&st, 0, sizeof(st));
		/* read regular file and symbol link */
		if ((-1 == stat(conf.maps[i].file, &st)) || !((S_IFREG & st.st_mode) || (S_IFLNK & st.st_mode)))
			continue;

		conf.maps[i].content = load_flv_file(conf.maps[i].file);
		if (conf.maps[i].content == NULL) continue;
		conf.maps[i].flv_file_ts = st.st_mtime;
	}
}

static int
parse_flvreaderconf(char *file)
{
	struct xmlnode *mainnode, *config, *p;
	int i, j, status = 1;

	if (file == NULL || file[0] == '\0') return 1;

	mainnode = xmlloadfile(file);
	if (mainnode == NULL) return 1;

	config = xmlgetchild(mainnode, "config", 0);

	conf.port = 81; /* default is 81 */
	conf.udp_port = 8889;
	conf.bindhost[0] = '\0';
	conf.map_count = 0;
	conf.maxconns = 32;
	conf.maps = NULL;

	if (config) {
		p = xmlgetchild(config, "maxconns", 0);
		if (p && p->value != NULL) {
			conf.maxconns = atoi(p->value);
			if (conf.maxconns <= 0) conf.maxconns = 32;
		}

		p = xmlgetchild(config, "port", 0);
		if (p && p->value != NULL) {
			conf.port = atoi(p->value);
			if (conf.port <= 0) conf.port = 81;
		}

		p = xmlgetchild(config, "udp-port", 0);
		if (p && p->value != NULL) {
			conf.udp_port = atoi(p->value);
			if (conf.udp_port <= 0) conf.udp_port = 8889;
		}

		p = xmlgetchild(config, "bind", 0);
		if (p && p->value != NULL) {
			strncpy(conf.bindhost, p->value, 32);
		}

		j = xmlnchild(config, "map");
		if (j > 0) {
			conf.maps = (struct urlmap *) calloc(sizeof(struct urlmap), j);
			if (conf.maps == NULL) {
				fprintf(stderr, "out of memory for parse_supervisorconf()\n");
				j = 0;
			} else {
				status = 0;
			}

			for (i = 0; i < j; i ++) {
				char *q;

				p = xmlgetchild(config, "map", i);
				if (p == NULL) {
					status = 0;
					break;
				}

				q = xmlgetattr(p, "url");
				if (q == NULL) {
					status = 0;
					break;
				}
				conf.maps[i].url = strdup(q);

				q = xmlgetattr(p, "file");
				if (q == NULL) {
					status = 0;
					break;
				}
				conf.maps[i].file = strdup(q);

				q = xmlgetattr(p, "backup");
				if (q == NULL) {
					conf.maps[i].backup = NULL;
				} else {
					conf.maps[i].backup = strdup(q);
				}

				conf.maps[i].use_backup = 0;
			}

			conf.map_count = i;
		}
	}

	freexml(mainnode);

	if (status == 0) {
		client_conns = (conn *)calloc(sizeof(conn), conf.maxconns);
		if (client_conns == NULL) {
			fprintf(stderr, "(%s.%d) out of memory for parse_readerconf()\n", __FILE__, __LINE__);
			status = 1;
		}
	}

	return status;
}

/* return 0 if added, return 1 if bad arguments */
static int
parse_flvreader_timer(char *args, int repeat)
{
	char *p, *q, *r, *s;
	struct timer *t;

	if (args == NULL || args[0] == '\0') return 1;

	/* time url action */
	p = args;
	q = strchr(p, ' ');

	if (q == NULL) return 1;

	r = strchr(q+1, ' ');
	if (r == NULL) return 1;

	t = (struct timer *) calloc(sizeof(struct timer), 1);
	if (t == NULL) return 1;

	t->repeat = repeat;

	*r = '\0';
	/* url */
	*q = '\0';
	t->url = strdup(q+1);

	*r = ' '; // restore
	r ++;
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
	if (strcasecmp(r, "usebackup") == 0) {
		t->action = 0;
	} else if (strcasecmp(r, "usemain") == 0) {
		t->action = 1;
	} else {
		free(t->url);
		free(t);
		return 1;
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

#define BUFLEN 4096
static void
flvreader_udp_handler(const int fd, const short which, void *arg)
{
	struct sockaddr_in server;
	char buf[BUFLEN+1], buf2[BUFLEN+1], *url;
	int r, i;
	socklen_t structlength;
	struct timer *t;

	structlength = sizeof(server);
	r = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *) &server, &structlength);
	if (r == -1) return;

	buf[r] = '\0';
	if (buf[r-1] == '\n') buf[--r] ='\0';

	if (strncasecmp(buf, "stat", 4) == 0) {
		r = snprintf(buf2, BUFLEN, "YOUKU Flash Reader v%s, NOW: %s, #%d MAPS\n", VERSION, cur_ts_str, conf.map_count);
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
		for (i = 0; i < conf.map_count; i ++) {
			if (conf.maps[i].backup) {
				if (conf.maps[i].use_backup) {
					r = snprintf(buf2, BUFLEN, "#%d \"%s\" -> BACKUP \"%s\", MAIN \"%s\"\n",
							i + 1, conf.maps[i].url, conf.maps[i].backup, conf.maps[i].file);
				} else {
					r = snprintf(buf2, BUFLEN, "#%d \"%s\" -> MAIN \"%s\", BACKUP \"%s\"\n",
							i + 1, conf.maps[i].url, conf.maps[i].file, conf.maps[i].backup);
				}
			} else {
				r = snprintf(buf2, BUFLEN, "#%d \"%s\" -> MAIN \"%s\"\n",
						i + 1, conf.maps[i].url, conf.maps[i].file);
			}
			sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
		}

		t = timer_header;
		i = 1;
		while (t) {
			r = snprintf(buf2, BUFLEN, "TIMER #%d: TIME %02d:%02d, URL \"%s\", ACTION: %s, %s\n",
					i, t->ts/3600, (t->ts%3600)/60,
					t->url,
					t->action?"TO MAIN":"TO BACKUP",
					t->repeat?"Repeat":"Once"
					);
			sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
			t = t->next;
			++ i;
		}
	} else if (r > 7 && strncasecmp(buf, "backup ", 7) == 0) {
		/* backup url */
		url = buf + 7;
		r = 0;
		for (i = 0; i < conf.map_count; i ++) {
			if (conf.maps[i].backup && (strcmp(conf.maps[i].url, url) == 0)) {
				/* found match */
				if (conf.maps[i].use_backup == 0) {
					r = snprintf(buf2, BUFLEN, "#%d %s -> BACKUP %s\n", i + 1, conf.maps[i].url, conf.maps[i].backup);
					conf.maps[i].use_backup = 1;
				} else {
					r = snprintf(buf2, BUFLEN, "#%d %s -> ALREADY IN BACKUP %s\n", i + 1, conf.maps[i].url, conf.maps[i].backup);
				}
				break;
			}
		}
		if (r == 0) r = snprintf(buf2, BUFLEN, "URL %s NOT FOUND OR HAS NO BACKUP\n", url);
		if (r > 0) sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (r > 8 && strncasecmp(buf, "restore ", 8) == 0) {
		/* restore url */
		url = buf + 8;
		r = 0;
		for (i = 0; i < conf.map_count; i ++) {
			if (strcmp(conf.maps[i].url, url) == 0) {
				/* found match */
				if (conf.maps[i].use_backup == 1) {
					r = snprintf(buf2, BUFLEN, "#%d %s -> MAIN %s\n", i + 1, conf.maps[i].url, conf.maps[i].file);
					conf.maps[i].use_backup = 0;
				} else {
					r = snprintf(buf2, BUFLEN, "#%d %s -> ALREADY IN MAIN %s\n", i + 1, conf.maps[i].url, conf.maps[i].file);
				}
				break;
			}
		}
		if (r == 0) r = snprintf(buf2, BUFLEN, "%s MAP NOT FOUND\n", url);
		if (r > 0) sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "cleartimer", 10) == 0) {
		struct timer *t;
		if (timer_header == NULL) {
			r = snprintf(buf2, BUFLEN, "NO TIMER\n");
		} else {
			while (timer_header) {
				t = timer_header->next;
				free(timer_header->url);
				free(timer_header);
				timer_header = t;
			}

			timer_tail = timer_header;
			r = snprintf(buf2, BUFLEN, "TIMER CLEARED\n");
		}
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "repeat ", 7) == 0) {
		if (parse_flvreader_timer(buf + 7, 1) == 0) {
			r = snprintf(buf2, BUFLEN, "%s -> TIMER OK\n", buf + 7);
		} else {
			r = snprintf(buf2, BUFLEN, "TRY: TIME URL ACTION\n");
		}
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "timer ", 6) == 0) {
		if (parse_flvreader_timer(buf + 6, 0) == 0) {
			r = snprintf(buf2, BUFLEN, "%s -> TIMER OK\n", buf + 6);
		} else {
			r = snprintf(buf2, BUFLEN, "TRY: TIME URL ACTION\n");
		}
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else if (strncasecmp(buf, "help", 4) == 0) {
		r = snprintf(buf2, BUFLEN, "status timer cleartimer repeat restore backup\n");
		sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
	} else {
		sendto(fd, NULL, 0, 0, (struct sockaddr *) &server, sizeof(server));
	}
}

int
main(int argc, char **argv)
{
	int sockfd, c, to_daemon = 1, udpfd = -1;
	struct sockaddr_in server, udpaddr;
	struct event ev_master, ev_udp;
	char *configfile = "/etc/flash/flvreader.xml";
	struct timeval tv;

	while(-1 != (c = getopt(argc, argv, "hDvc:s"))) {
		switch(c)  {
		case 'D':
			to_daemon = 0;
			break;
		case 'v':
			verbose_mode = 1;
			break;
		case 's':
			use_seekable = 1;
			break;
		case 'c':
			configfile = optarg;
			break;
		case 'h':
		default:
			show_reader_help();
			return 1;
		}
	}

	if (configfile == NULL || configfile[0] == '\0') {
		fprintf(stderr, "Please provide -c file argument, or use default '/etc/flash/flvreader.xml' \n");
		show_reader_help();
		return 1;
	}

	if (parse_flvreaderconf(configfile)) {
		fprintf(stderr, "(%s.%d) parse xml %s failed\n", __FILE__, __LINE__, configfile);
		return 1;
	}

	init_flvfiles();

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
			fprintf(stderr, "(%s.%d) errno = %d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
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

	fprintf(stderr, "%s: Youku Live FLV Reader v%s(" __DATE__ " " __TIME__ "), HTTPD: TCP %s:%d\n",
			cur_ts_str, VERSION, conf.bindhost[0]?conf.bindhost:"0.0.0.0", conf.port);

	signal(SIGPIPE, SIG_IGN);

	if (to_daemon && daemonize(0, 0) == -1) {
		fprintf(stderr, "(%s.%d) failed to be a daemon\n", __FILE__, __LINE__);
		exit(1);
	}

	event_init();

	/* timer */
	evtimer_set(&ev_timer, reader_timer_service, NULL);
	tv.tv_sec = 1; tv.tv_usec = 0; /* check for every 1 seconds */
	event_add(&ev_timer, &tv);

	/* tcp */
	event_set(&ev_master, sockfd, EV_READ|EV_PERSIST, flvreader_client_accept, NULL);
	event_add(&ev_master, 0);

	/* udp */
	event_set(&ev_udp, udpfd, EV_READ|EV_PERSIST, flvreader_udp_handler, NULL);
	event_add(&ev_udp, 0);

	event_dispatch();
	return 0;
}
