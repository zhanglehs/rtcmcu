/* source code of handling request of live stream client
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

#include <inttypes.h>
#include "transporter.h"

static struct event ev_master;
struct event_base *main_base = NULL;
static struct event ev_timer;
int allow_live_service = 1;

/* transporter.c */
extern time_t cur_ts;
extern char cur_ts_str[128];
extern int mainfd, verbose_mode;
extern struct connection *client_conns;
extern struct config conf;

/* control.c */
extern struct stream * find_stream(int, int);
extern struct stream * switch_global_stream(int, int, int);
extern void check_global_streams_waitqueue(void);

/* backend.c */
extern int connect_live_stream_server(struct stream *);

/* status code */
static keyvalue http_status_str[] =
{
    { 200, "OK" },
    { 206, "Partial Content" },
    { 301, "Moved Permanently" },
    { 302, "Found" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 502, "Bad Gateway" },
    { 503, "Service Not Available" },
    { 504, "Gateway Timeout" },
    { 505, "HTTP Version Not Supported" },

    { -1, NULL }
};

#define NOCACHE_HEADER "Expires: -1\r\nCache-Control: private, max-age=0\r\nPragma: no-cache\r\n"
#define MINIMUM_P2P_DURATION 940

void conn_close(conn *);
static void http_handler(const int, const short, void *);
static void stream_client(conn *);

static const char resivion[] __attribute__((used)) = { "$Id: client.c 541 2012-04-28 02:01:35Z qhy $" };

static void
send_report(conn *c, int action)
{
    struct report r;
    struct sockaddr_in srv;

    if (c == NULL || c->remote_ip[0] == '\0' || conf.report_fd <= 0) return;

    memset(&r, 0, sizeof(r));
    r.port = c->remote_port;
    r.ts = cur_ts;
    r.bps = c->bps;
    r.stream = c->streamid;

    if (c->stream_mode == TS_MODE) ++ r.bps;

    r.code = REPORT_MAGIC_CODE;
    r.action = action;
    r.sid = c->sid;
    srv.sin_addr.s_addr = inet_addr(c->remote_ip);
    memcpy(&r.ip, &(srv.sin_addr.s_addr), sizeof(r.ip));

    sendto(conf.report_fd, &r, sizeof(r), 0, (struct sockaddr *) &conf.report_server, sizeof(struct sockaddr_in));
}

static const char *
keyvalue_get_value(keyvalue *kv, int k)
{
    int i;
    for (i = 0; kv[i].value; i++) {
        if (kv[i].key == k) return kv[i].value;
    }
    return NULL;
}

static char crossdomain[200] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
    "<cross-domain-policy>\r\n"
    "\t<allow-access-from domain=\"*\"/>\r\n"
    "</cross-domain-policy>\r\n";

static void
write_http_error(conn *c)
{
    char errstr[513];
    int r;

    if (c == NULL || c->http_status == 0) return;

    if (c->is_crossdomainxml == 0)
        r = snprintf(errstr, 255, "HTTP/1.0 %d %s\r\nConnection: close\r\n\r\n", c->http_status,
                keyvalue_get_value(http_status_str, c->http_status));
    else {
        r = snprintf(errstr, 512, "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Length: %d\r\n"
                "Expires: Mon, 18 Feb 2018 16:57:25 GMT\r\n"
                "Cache-Control: max-age=2592000\r\n"
                "\r\n"
                "%s",
            (int) strlen(crossdomain), crossdomain);
    }

    write(c->fd, errstr, r);
    conn_close(c);
}

static void
parse_request_line(conn *c)
{
    char *p;
    if (c == NULL) return;

    p = c->r->ptr + 4;
    if (strncmp(c->r->ptr, "GET ", 4) != 0 || p[0] != '/') {
        c->http_status = 400; /* Bad Request */
        return;
    }

    /* GET /1/250/[0|33333]/xxxxx HTTP/1.1\r\n
     * GET /stream/bitrate/starttime/sid HTTP/1.1\r\n
     * GET /stream/bitrate/sid/starttime.ts HTTP/1.1\r\n
     * GET /mstream/bitrate/sid.m3u8 HTTP/1.1\r\n
     */

    /* GET /yklive/xxx, remove /yklive/ prefix */

    /* p2p mode
     * GET /pstream/bitrate/sid/starttime/blocks.flv
     */

    c->streamid = c->bps = c->offset = c->sid = 0;

    /* "GET /" */
    p = c->r->ptr + 5;
    if (strncmp(p, "crossdomain.xml", 15) == 0) {
        c->is_crossdomainxml = 1;
        c->http_status = 200;
        return;
    }

#define P2P_PREFIX "youkulive/"
#define P2P_PREFIX_LEN 10

    if (strncmp(p, P2P_PREFIX, P2P_PREFIX_LEN) == 0) {
        /* skip prefix */
        p += P2P_PREFIX_LEN;
    }

    if (p[0] == 'm') {
        c->stream_mode = M3U8_MODE;
        p ++;
    } else if (p[0] == 't') {
        c->stream_mode = TS_MODE;
        p ++;
    } else if (p[0] == 'p') {
        c->stream_mode = P2P_MODE;
        p ++;
    } else {
        c->stream_mode = LIVE_MODE;
    }

    c->streamid = c->bps = c->sid = c->offset = 0;
    c->blocks = -1;

    while (p) {
        /* stream */
        if (p[0] >= '0' && p[0] <= '9') {
            c->streamid = c->streamid * 10 + p[0] - '0';
            p ++;
        } else {
            if (p[0] != '/') {
                c->http_status = 400;
            } else {
                p ++;
            }
            break;
        }
    }

    if (c->http_status == 0) {
        /* bit rate */
        while (p) {
            if (p[0] >= '0' && p[0] <= '9') {
                c->bps = c->bps * 10 + p[0] - '0';
                p ++;
            } else {
                break;
            }
        }

        switch (c->stream_mode) {
        case LIVE_MODE:
            /* offset */
            if (p[0] != '/' && p[0] != ' ') {
                c->http_status = 400;
            } else if (p[0] == '/') {
                p ++;
                /* offset */
                while (p) {
                    if (p[0] >= '0' && p[0] <= '9') {
                        c->offset = c->offset * 10 + p[0] - '0';
                        p ++;
                    } else {
                        break;
                    }
                }
            }

            /* sid */
            if (p[0] != '/' && p[0] != ' ') {
                c->http_status = 400;
            } else if (p[0] == '/') {
                p ++;
                /* sid */
                while (p) {
                    if (p[0] >= '0' && p[0] <= '9') {
                        c->sid = c->sid * 10 + p[0] - '0';
                        p ++;
                    } else {
                        break;
                    }
                }
            }
            break;
        case TS_MODE:
            /* sid */
            if (p[0] != '/' && p[0] != ' ') {
                c->http_status = 400;
            } else if (p[0] == '/') {
                p ++;
                /* sid */
                while (p) {
                    if (p[0] >= '0' && p[0] <= '9') {
                        c->sid = c->sid * 10 + p[0] - '0';
                        p ++;
                    } else {
                        break;
                    }
                }
            }

            /* offset */
            if (p[0] != '/' && p[0] != ' ') {
                c->http_status = 400;
            } else if (p[0] == '/') {
                p ++;
                /* offset */
                while (p) {
                    if (p[0] >= '0' && p[0] <= '9') {
                        c->offset = c->offset * 10 + p[0] - '0';
                        p ++;
                    } else {
                        break;
                    }
                }
            }
            break;
        case P2P_MODE:
            /* sid */
            if (p[0] != '/' && p[0] != ' ') {
                c->http_status = 400;
            } else if (p[0] == '/') {
                p ++;
                /* sid */
                while (p) {
                    if (p[0] >= '0' && p[0] <= '9') {
                        c->sid = c->sid * 10 + p[0] - '0';
                        p ++;
                    } else {
                        break;
                    }
                }
            }

            /* offset */
            if (p[0] != '/' && p[0] != ' ') {
                c->http_status = 400;
            } else if (p[0] == '/') {
                p ++;
                /* offset */
                while (p) {
                    if (p[0] >= '0' && p[0] <= '9') {
                        c->offset = c->offset * 10 + p[0] - '0';
                        p ++;
                    } else {
                        break;
                    }
                }
            }

            /* blocks */
            if (p[0] != '/' && p[0] != ' ') {
                c->http_status = 400;
            } else if (p[0] == '/') {
                p ++;
                /* blocks */
                c->blocks = 0;
                while (p) {
                    if (p[0] >= '0' && p[0] <= '9') {
                        c->blocks = c->blocks * 10 + p[0] - '0';
                        p ++;
                    } else {
                        break;
                    }
                }
                if (c->blocks == 0) c->blocks = -1;
            }

            break;
        default:
            /* M3U8 Mode */
            /* sid */
            if (p[0] == '/') {
                p ++;
                /* sid */
                while (p) {
                    if (p[0] >= '0' && p[0] <= '9') {
                        c->sid = c->sid * 10 + p[0] - '0';
                        p ++;
                    } else {
                        break;
                    }
                }
            }
            break;
        }
    }
}

void
conn_close(conn *c)
{
    if (c == NULL) return;
    if (c->fd > 0) {
        if (verbose_mode) {
            if (c->s)
                ERROR_LOG(conf.logfp, "stream #%d/#%d: CLOSING CLIENT FD #%d(%s:%d)\n",
                        c->s->si.streamid, c->s->si.bps,
                        c->fd, c->remote_ip, c->remote_port);
            else
                ERROR_LOG(conf.logfp, "CLOSING CLIENT #%d(%s:%d)\n", c->fd, c->remote_ip, c->remote_port);
        }
        event_del(&(c->ev));
        close(c->fd);
        c->fd = 0;
    }

    if (c->s && c->stream_mode == LIVE_MODE) send_report(c, REPORT_LEAVE);

    c->s = NULL;
    c->live = NULL;
    if (c->r) c->r->used = 0;
    if (c->w) c->w->used = 0;

    c->is_first_line = 0;
    c->is_crossdomainxml = 0;

    c->streamid = c->bps = c->offset = c->sid = 0;
    c->blocks = -1;
    c->stream_mode = LIVE_MODE;
    c->to_close = 0;

    c->http_status = 0;
    c->wpos = 0;
    c->in_waitqueue = 0;

    c->remote_ip[0] = '\0'; c->remote_port = 0;

    c->seek_to_keyframe = 0;
    c->expire_ts = 0;
    c->report_ts = 0;

    c->live_start = c->live_end = c->live_end_ts = 0;
}

static void
handle_m3u8_request(conn *c)
{
    char date[256];
    struct buffer *b, *b2;
    int duration = 0;
    int squence = 0, i = 0;
    struct ts_chunkqueue *cq;
    struct stream *s;

    if (c == NULL) return;
    if (c->s == NULL || c->streamid == 0 || c->bps == 0 || c->s->max_ts_count == 0) {
        c->http_status = 404;
        return;
    }

    s = c->s;
    b = buffer_init(512);
    b2 = buffer_init(256);
    if (b == NULL || b2 == NULL) {
        buffer_free(b);
        buffer_free(b2);
        c->http_status = 504;
        return;
    }

    c->w->used = 0;
    c->to_close = 1;
    strftime(date, sizeof("Fri, 01 Jan 1990 00:00:00 GMT")+1,
            "%a, %d %b %Y %H:%M:%S GMT", gmtime(&cur_ts));

    if (c->sid == 0) {
        /* generate new random session id */
        c->sid = random();
        /* prepare http response header */
        if (c->local_port == 0) {
            b2->used = snprintf(b2->ptr, 256,
                "#EXTM3U\n#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=%d\n"
                "/m%d/%d/%" PRIu64 ".m3u8\n",
                c->bps * 1024,
                (int) c->streamid, (int) c->bps, c->sid);
            b->used = snprintf(b->ptr, 512, "HTTP/1.0 200 OK\r\nConnection: close\r\n"
                NOCACHE_HEADER
                "Content-Type: application/vnd.apple.mpegurl\r\n"
                "Content-Length: %d\r\n"
                "Date: %s\r\n\r\n",
                b2->used, date);
        } else {
            b2->used = snprintf(b2->ptr, 256,
                "#EXTM3U\n#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=%d\n"
                "http://%s:%d/m%d/%d/%" PRIu64 ".m3u8\n",
                c->bps * 1024,
                c->local_ip, c->local_port, (int) c->streamid, (int) c->bps, c->sid);

            b->used = snprintf(b->ptr, 512, "HTTP/1.1 200 OK\r\nConnection: close\r\n"
                NOCACHE_HEADER
                "Content-Type: application/vnd.apple.mpegurl\r\n"
                "Host: %s:%d\r\n"
                "Content-Length: %d\r\n"
                "Date: %s\r\n\r\n",
                c->local_ip, c->local_port, b2->used, date);
        }
        buffer_copy(c->w, b);
        buffer_copy(c->w, b2);
    } else {
        b->used = 0;
        if (s->cq_number > 0) {
            if (s->ts_reset_ticker > 0) {
                b2->used = snprintf(b2->ptr, 256, "#EXT-X-DISCONTINUITY\n");
                buffer_append(b, b2);
                cq = s->ipad_tail; /* only newest ts segment */
            } else {
                i = s->cq_number;
                cq = s->ipad_head;
                while (i > 3 && cq) {
                    cq = cq->next;
                    -- i;
                }
            }

            while (cq) {
                i = (int)(cq->duration / 1000.0 + 0.5);
                if (squence == 0) squence = cq->squence;

                if (c->local_port == 0) {
                    b2->used = snprintf(b2->ptr, 256, "#EXTINF:%d,\n/t%d/%d/%" PRIu64 "/%lu/%d.ts\n",
                        i, (int) c->streamid, (int) c->bps, c->sid, cq->ts, cq->squence);
                } else {
                    b2->used = snprintf(b2->ptr, 256, "#EXTINF:%d,\nhttp://%s:%d/t%d/%d/%" PRIu64 "/%lu/%d.ts\n",
                        i, c->local_ip, c->local_port,
                        (int) c->streamid, (int) c->bps, c->sid, cq->ts, cq->squence);
                }
                buffer_append(b, b2);
                cq = cq->next;
            }
        } else {
            duration = 0;
            b2->used = snprintf(b2->ptr, 256, "#EXT-X-ENDLIST\n");
            buffer_append(b, b2);
        }

        b2->used = snprintf(b2->ptr, 256, "#EXTM3U\n#EXT-X-TARGETDURATION:%d\n#EXT-X-MEDIA-SEQUENCE:%u\n\n",
                (int) (s->max_ts_duration / 1000.0 + 0.5), squence);
        buffer_append(b2, b);

        if (c->local_port) {
            b->used = snprintf(b->ptr, 512, "HTTP/1.1 200 OK\r\nConnection: close\r\n"
                NOCACHE_HEADER
                "Content-Type: application/vnd.apple.mpegurl\r\n"
                "Host: %s:%d\n"
                "Content-Length: %d\r\n"
                "Date: %s\r\n\r\n",
                c->local_ip, c->local_port,
                b2->used, date);
        } else {
            b->used = snprintf(b->ptr, 512, "HTTP/1.0 200 OK\r\nConnection: close\r\n"
                NOCACHE_HEADER
                "Content-Type: application/vnd.apple.mpegurl\r\n"
                "Content-Length: %d\r\n"
                "Date: %s\r\n\r\n",
                b2->used, date);
        }

        buffer_append(c->w, b);
        buffer_append(c->w, b2);
    }

    buffer_free(b);
    buffer_free(b2);
}

static void
handle_ts_request(conn *c)
{
    char date[256];
    buffer *b;
    struct ts_chunkqueue *cq;

    if (c == NULL) return;
    if (c->s == NULL || c->streamid == 0 || c->bps == 0) {
        c->http_status = 404;
        return;
    }

    cq = c->s->ipad_head;
    while (cq) {
        if (c->offset == cq->ts) break;
        cq = cq->next;
    }

    if (cq == NULL) {
        c->http_status = 404;
        return;
    }

    b = buffer_init(512);
    if (b == NULL) {
        c->http_status = 504;
        return;
    }

    c->w->used = 0;
    c->to_close = 1;
    strftime(date, sizeof("Fri, 01 Jan 1990 00:00:00 GMT")+1,
            "%a, %d %b %Y %H:%M:%S GMT", gmtime(&cur_ts));

    if (c->local_port) {
        b->used = snprintf(b->ptr, 512, "HTTP/1.1 200 OK\r\nConnection: close\r\n"
            NOCACHE_HEADER
            "Content-Type: video/x-mpeg-ts\r\n"
            "Host: %s:%d\r\n"
            "Content-Length: %d\r\n"
            "Date: %s\r\n\r\n",
            c->local_ip, c->local_port,
            cq->mem->used, date);
    } else {
        b->used = snprintf(b->ptr, 512, "HTTP/1.0 200 OK\r\nConnection: close\r\n"
            NOCACHE_HEADER
            "Content-Type: video/x-mpeg-ts\r\n"
            "Content-Length: %d\r\n"
            "Date: %s\r\n\r\n",
            cq->mem->used, date);
    }

    buffer_append(c->w, b);
    buffer_append(c->w, cq->mem);
    buffer_free(b);

    send_report(c, REPORT_KEEPALIVE);
    c->report_ts = cur_ts + conf.report_interval;
}

static void
handle_live_request(conn *c)
{
    int length;
    char date[65];
    struct stream *s = NULL;

    if (c == NULL) return;

    if (c->s == NULL || c->streamid == 0 || c->bps == 0) {
        c->http_status = 404;
        return;
    }

    s = c->s;
    /* prepare http response header */
    strftime(date, sizeof("Fri, 01 Jan 1990 00:00:00 GMT")+1,
            "%a, %d %b %Y %H:%M:%S GMT", gmtime(&cur_ts));
    length = c->bps;
    length *= 1024 * 3600 * 8 / 8; /* 8 hours */
    c->w->used = snprintf(c->w->ptr, c->w->size,
        "HTTP/1.0 200 OK\r\nDate: %s\r\nServer: Youku Live Transformer\r\n"
        "Content-Length: %u\r\nContent-Type: video/x-flv\r\n"
        NOCACHE_HEADER
        "\r\n",
        date, length);

#define FLVHEADER "FLV\x1\x5\0\0\0\x9\0\0\0\0" /* 9 plus 4bytes of UINT32 0 */
#define FLVHEADER_SIZE 13
    /* we need to fill standard flv header */
    memcpy(c->w->ptr + c->w->used, FLVHEADER, FLVHEADER_SIZE /* sizeof(FLVHEADER) - 1 */);
    c->w->used += FLVHEADER_SIZE;
#undef FLVHEADER
#undef FLVHEADER_SIZE

    buffer_append(c->w, s->onmetadata);
    buffer_append(c->w, s->avc0);
    buffer_append(c->w, s->aac0);

    send_report(c, REPORT_JOIN);
    c->report_ts = cur_ts + conf.report_interval;
    c->live = NULL;

    if (c->offset > 86400) {
        /* it's unix starttime */
        length = s->start_ts / 1000;
        if (c->offset < length) c->offset = length;

        /* don't use cur_ts because live stream's timestamp lags behind current */
        length = (s->start_ts + s->last_stream_ts) / 1000;
        if (length > c->offset) c->offset = length - c->offset;
        else c->offset = 0;
    }

    if (c->offset > (2 * conf.live_buffer_time)) c->offset = conf.live_buffer_time * 2;
    c->offset *= 1000;
    if (c->offset > 0) c->seek_to_keyframe = 0;
}

/* return -1 if not found */
static int
find_start_pos(struct segment *s, time_t ts)
{
    int low = 0, i, high;

    if (s == NULL || s->map_used <= 1) return -1;
    if (s->map[0].ts > ts || s->map[s->map_used-1].ts < ts) return -1;

    high = s->map_used - 1;
    while (low <= high) {
        i = (low + high) / 2;
        if (s->map[i].ts > ts) {
            high = i - 1;
        } else if (s->map[i].ts == ts) {
            return i;
        } else {
            low = i + 1;
        }
    }
    return low;
}

static buffer *
find_p2p_buffer(struct stream *s, uint64_t ts)
{
    int start = -1, end = -1, is_head = 0, i;
    uint32_t ts_diff;
    struct segment *seg;
    buffer *b, t;

    if (s == NULL || ts == 0 || s->head == NULL || s->tail == NULL) return NULL;

    ts_diff = ts - s->start_ts;

    if (ts_diff > s->tail->last_ts || ts_diff < s->head->first_ts) {
        return NULL;
    }

    /* ts is round to * 1000 */
    if (s->head == s->tail) {
        seg = s->tail;
    } else if ((ts - s->start_ts) > s->head->last_ts) {
        /* in tail */
        seg = s->tail;
    } else {
        /* in head */
        seg = s->head;
        is_head = 1;
    }

    /* find start pos */
    start = find_start_pos(seg, ts_diff);
    if (start == -1)
        return NULL;

    ts_diff += 1000; /* 1 second */
    for (i = start + 1; i < seg->map_used - 1; i ++) {
        if (seg->map[i].ts < ts_diff && ts_diff <= seg->map[i+1].ts) {
            end = i;
            break;
        }
    }

    if (end == -1 && is_head == 0)
        return NULL;

    b = buffer_init(1024);
    if (b == NULL) return NULL;

    if (end == -1) {
        t.used = seg->memory->used - seg->map[start].start;
        t.size = t.used + 1;
        t.ptr = seg->memory->ptr + seg->map[start].start;
        buffer_copy(b, &t);
        seg = s->tail;
        for (i = 0; i < seg->map_used - 1; i ++) {
            if (seg->map[i].ts < ts_diff && ts_diff <= seg->map[i+1].ts) {
                end = i;
                break;
            }
        }
        if (end == -1) {
            /* not match */
            buffer_free(b);
            return NULL;
        }
        t.used = seg->map[end+1].start;
        t.size = t.used + 1;
        t.ptr = seg->memory->ptr;
        buffer_append(b, &t);
    } else {
        t.used = seg->map[end+1].start - seg->map[start].start;
        t.size = t.used + 1;
        t.ptr = seg->memory->ptr + seg->map[start].start;
        buffer_copy(b, &t);
    }

    return b;
}

static void
parse_p2p_buffer(conn *c, buffer *b)
{
    FLVTag *tag;
    int pos = 0, tagsize = 0, len;
    time_t start_ts = -1, ts, new_ts = 0, prev_video_ts = -1, off_ts = 0;

    if (c == NULL || b == NULL || b->ptr == NULL) return;
    c->p2p_flag = 0;
    c->p2p_duration = 0;

    while (pos < b->used) {
        /* find off_ts first */
        tag = (FLVTag *)(b->ptr + pos);
        tagsize = (tag->datasize[0] << 16) + (tag->datasize[1] << 8) +
                    tag->datasize[2] + sizeof(FLVTag) + 4;
        ts = (tag->timestamp_ex << 24) + (tag->timestamp[0] << 16) + (tag->timestamp[1] << 8) + tag->timestamp[2];
        len = sizeof(FLVTag);
        if (tag->type == FLV_AUDIODATA) {
            if (((b->ptr[pos + len] & 0xf0) != 0xA0) || (b->ptr[pos + len + 1] != 0)) {
                /* not AAC0 */
                off_ts = (ts + c->s->start_ts - c->p2p_ts * 1000);
                break;
            }
        } else if (tag->type == FLV_VIDEODATA ) {
            if (((b->ptr[len + pos] & 0xf) != 0x7) || (b->ptr[pos + len + 1] != 0)) {
                /* not AVC0 */
                off_ts = (ts + c->s->start_ts - c->p2p_ts * 1000);
                break;
            }
        }
        pos += tagsize;
    }

    pos = 0;
    while (pos < b->used) {
        tag = (FLVTag *)(b->ptr + pos);
        tagsize = (tag->datasize[0] << 16) + (tag->datasize[1] << 8) +
                    tag->datasize[2] + sizeof(FLVTag) + 4;
        len = sizeof(FLVTag);
        ts = (tag->timestamp_ex << 24) + (tag->timestamp[0] << 16) + (tag->timestamp[1] << 8) + tag->timestamp[2];
        new_ts = 0;
        if (tag->type == FLV_AUDIODATA) {
            if (((b->ptr[pos + len] & 0xf0) == 0xA0) && (b->ptr[pos + len + 1] == 0)) {
                c->p2p_flag |= P2P_HAS_AAC0;
            } else {
                if ((b->ptr[pos + len] & 0xf0) == 0x20) { /* MP3 audio */
                    c->p2p_flag |= P2P_MP3_AUDIO;
                }

                if (start_ts == -1) {
                    start_ts = ts;
                } else {
                    new_ts = ts - start_ts;
                    c->p2p_duration = new_ts;
                }
            }
        } else if (tag->type == FLV_VIDEODATA ) {
            if (((b->ptr[len + pos] & 0xf) == 0x7) && (b->ptr[pos + len + 1] == 0)) {
                c->p2p_flag |= P2P_HAS_AVC0;
            } else {
                if (((b->ptr[pos + len] & 0xf0) == 0x10) || ((b->ptr[pos + len] & 0xf0) == 0x40)) {
                        c->p2p_flag |= P2P_HAS_KEYFRAME;
                }

                if (start_ts == -1) {
                    start_ts = ts;
                } else {
                    new_ts = ts - start_ts;
                    c->p2p_duration = new_ts;
                }

                if (prev_video_ts == -1) {
                    prev_video_ts = new_ts;
                } else {
                    if ((new_ts - prev_video_ts) > 100) { /* 0.1s */
                            c->p2p_flag |= P2P_HAS_HOLE;
                    } else if (new_ts < prev_video_ts) { /* leap back */
                            c->p2p_flag |= P2P_HAS_HOLE;
                    }
                    prev_video_ts = new_ts;
                }
            }
        } else if (tag->type == FLV_SCRIPTDATAOBJECT ) {
            c->p2p_flag |= P2P_HAS_METADATA;
        }

        /* we need to update flv stream's timestamp, starting from zero */
        new_ts += off_ts;
        tag->timestamp_ex = (new_ts >> 24) & 0xff;
        tag->timestamp[0] = (new_ts >> 16) & 0xff;
        tag->timestamp[1] = (new_ts >> 8) & 0xff;
        tag->timestamp[2] = new_ts & 0xff;
        pos += tagsize;
    }
}

static void
handle_p2p_request(conn *c)
{
    int length, first_packet = 1;
    char date[65];
    struct stream *s = NULL;
    struct stream_map *m = NULL;
    uint64_t ts;
    uint32_t ts_diff;
    buffer *b, *b2, t;
    struct p2p_header p;

    if (c == NULL) return;

    if (c->s == NULL || c->streamid == 0 || c->bps == 0 || c->s->tail == NULL) {
        /* make sure that we have data */
        c->http_status = 404;
        return;
    }

    s = c->s;
    if (c->offset > 0) {
        ts = c->offset * 1000; /* in ms */
        ts_diff = ts - s->start_ts;
        if (ts_diff > s->tail->last_ts || ts_diff < s->head->first_ts) {
            /* not match */
            c->http_status = 404;
            return;
        }
        first_packet = 0;
    } else {
        if (s->tail->keymap_used == 0) {
            /* no newest keyframe */
            c->http_status = 404;
            return;
        }

        if (s->tail->keymap_used > 2) {
            m = s->tail->keymap + s->tail->keymap_used - 2;
        } else {
            m = s->tail->keymap;
        }
        ts = ((m->ts + s->start_ts)/1000 - 1) * 1000;  /* round to nearest old second */
    }

    /* we have buffer now */
    b = find_p2p_buffer(s, ts);
    if (b == NULL) {
        c->http_status = 404;
        return;
    }

    /* to parse p2p buffer now */
    b2 = buffer_init(1024);
    if (b2 == NULL) {
        c->http_status = 404;
        buffer_free(b);
        return;
    }

    if (first_packet == 1) {
        buffer_append(b2, s->onmetadata);
        buffer_append(b2, s->avc0);
        buffer_append(b2, s->aac0);
    }
    buffer_append(b2, b);

    c->p2p_ts = ts / 1000;
    parse_p2p_buffer(c, b2);
    buffer_free(b);

    memset(&p, 0, sizeof(p));
    p.magic[0] = 'P'; p.magic[1] = 'L';
    p.packet_number = c->p2p_ts;
    p.length = sizeof(p) + b2->used;
    if (c->p2p_duration > MINIMUM_P2P_DURATION && ((c->p2p_flag & P2P_HAS_HOLE) == 0)) {
        /* duration > 0.95s for first p2p pacet */
        c->p2p_flag += P2P_IS_OK;
    }
    p.flag = c->p2p_flag;
    p.ver = 1;
    p.duration = c->p2p_duration;

    /* prepare http response header */
    strftime(date, sizeof("Fri, 01 Jan 1990 00:00:00 GMT")+1,
            "%a, %d %b %Y %H:%M:%S GMT", gmtime(&cur_ts));
    length = c->bps;
    length *= 1024 * 3600 * 8 / 8; /* 8 hours */
    c->w->used = snprintf(c->w->ptr, c->w->size,
        "HTTP/1.0 200 OK\r\nDate: %s\r\nServer: Youku Live Transformer\r\n"
        "Content-Length: %u\r\nContent-Type: video/p2p-flv\r\n"
        NOCACHE_HEADER
        "\r\n",
        date, length);

    t.used = sizeof(p);
    t.size = t.used + 1;
    t.ptr = (void *)&p;
    buffer_append(c->w, &t);
    buffer_append(c->w, b2);
    buffer_free(b2);

    if (verbose_mode)
        ERROR_LOG(conf.logfp, "client fd #%d(%s:%d) -> P2P at %.0f, flag 0x%x, duration %dms\n",
                c->fd, c->remote_ip, c->remote_port, c->p2p_ts * 1.0, c->p2p_flag, (int)c->p2p_duration);
    if (c->blocks > 0) {
        -- c->blocks;
        if (c->blocks == 0) c->to_close = 1; /* close client after N blocks */
    }
}

static void
handle_http_request(conn *c)
{
    struct stream *s = NULL;

    if (c == NULL || c->http_status != 0) return;

    if (allow_live_service == 0) {
        c->http_status = 503; /* Service Not Available */
        return;
    }

    if (c->streamid == 0 || c->bps == 0) {
        c->http_status = 404; /* NOT FOUND */
        return;
    }

    s = find_stream(c->streamid, c->bps);

    if (s == NULL) {
        c->http_status = 404; /* request stream and bps not found */
        return;
    }

    if (s->connected == 0) {
        connect_live_stream_server(s);
        if (s->tail == NULL) {
            c->http_status = 502; /* BAD GATEWAY */
            return;
        }
    }

    /* live stream is connected */
    c->s = s;
    c->wpos = 0;

    switch (c->stream_mode) {
    case M3U8_MODE:
        handle_m3u8_request(c);
        break;
    case TS_MODE:
        handle_ts_request(c);
        break;
    case P2P_MODE:
        handle_p2p_request(c);
        break;
    default:
        handle_live_request(c);
        break;
    }
}

static void
disable_write(conn *c)
{
    if (c && c->fd > 0 && c->is_writable == 1) {
        event_del(&c->ev);
        event_set(&c->ev, c->fd, EV_READ|EV_PERSIST, http_handler, (void *)c);
        event_add(&c->ev, 0);
        c->is_writable = 0;
    }
}

static void
enable_write(conn *c)
{
    if (c && c->fd > 0 && c->is_writable == 0) {
        c->is_writable = 1;
        event_del(&c->ev);
        event_set(&c->ev, c->fd, EV_READ|EV_WRITE|EV_PERSIST, http_handler, (void *)c);
        event_add(&c->ev, 0);
    }
}

static void
append_waitqueue(conn *c)
{
    struct stream *s;

    if (c == NULL || c->in_waitqueue == 1 || c->s == NULL) return;
    s = c->s;
    if (s->waitqueue == NULL || s->waitqueue->used >= s->waitqueue->size)
        return;

    s->waitqueue->conns[s->waitqueue->used ++] = c;
    c->in_waitqueue = 1;
    disable_write(c);
}

/* write data from live_start to live_end only */
static void
write_client(conn *c)
{
    int r = 0, diff, cnt = 0, i;
    struct iovec chunks[2];

    if (c == NULL || c->s == NULL) return;

    if (c->stream_mode != M3U8_MODE && c->report_ts < cur_ts) {
        send_report(c, REPORT_KEEPALIVE);
        c->report_ts = cur_ts + conf.report_interval;
    }

    if (c->wpos < c->w->used) {
        chunks[0].iov_base = c->w->ptr + c->wpos;
        chunks[0].iov_len = c->w->used - c->wpos;
        cnt = 1;
    }

#if 0
    if (c->live && c->live_end < c->live->memory->used) {
        c->live_end = c->live->memory->used;
    }
#endif

    if (c->stream_mode == LIVE_MODE && c->to_close == 0 && c->live && (c->live_start < c->live_end)) {
        chunks[cnt].iov_base = c->live->memory->ptr + c->live_start;
        r = c->live_end - c->live_start;
        if (r > conf.client_write_blocksize) r = conf.client_write_blocksize;
        chunks[cnt].iov_len = r;
        cnt ++;
    }

    if (cnt == 0) {
        /* no chunks to write, append to job queue */
        if (c->to_close) conn_close(c);
        else append_waitqueue(c);
        return;
    }

    r = writev(c->fd, (struct iovec *)&chunks, cnt);
    if (r < 0) {
            if (errno != EINTR && errno != EAGAIN) {
            conn_close(c);
            return;
        }
        r = 0;
    }

    if (conf.client_expire_time > 0) c->expire_ts = cur_ts + conf.client_expire_time; /* update client expire time */

    if (c->w->used > 0 && c->wpos <= c->w->used) {
        if (r >= (c->w->used - c->wpos)) {
            r -= (c->w->used - c->wpos);
            c->wpos = c->w->used = 0;
        } else {
            c->wpos += r;
            r = 0;
        }
    }

    if (c->to_close) {
        if (c->w->used == 0) conn_close(c);
        else append_waitqueue(c);
        return;
    }

    if (c->stream_mode == LIVE_MODE) {
        c->live_start += r;

        if (c->live_start >= c->live_end) {
            /* finish write of segment of flv frame */
            if (conf.client_max_lag_seconds > 0 && c->live->keymap_used > 0 ) {
                diff = c->live->keymap[c->live->keymap_used-1].ts - c->live_end_ts;

                if (diff > conf.client_max_lag_seconds) {
                    /* start from next keyframe */
                    if (verbose_mode) {
                        ERROR_LOG(conf.logfp, "client fd #%d(%s:%d) -> to fly, diff/last/client/keymap %d/%u/%lu/%u\n",
                                c->fd, c->remote_ip, c->remote_port, diff,
                                c->live->last_ts / 1000, c->live_end_ts / 1000,
                                c->live->keymap[c->live->keymap_used-1].ts / 1000);
                    }

                    c->live_start = c->live->keymap[c->live->keymap_used - 1].start;
                    i = c->live->map_used - 1;
                    c->live_end = c->live->map[i].end;
                    c->live_end_ts = c->live->map[i].ts;
                }
            }

            append_waitqueue(c);
        } else {
            /* there has data not written to clients */
            enable_write(c);
        }
    } else if (c->stream_mode == P2P_MODE) {
        if (c->w->used == 0) {
            append_waitqueue(c);
        } else {
            enable_write(c);
        }
    }
}

/* when offset <= seg->first_ts, return 0
 * when offset > seg->last_ts, return -1
 * otherwise find matched keymap position
 */
static int
find_keyframe(struct segment *seg, uint32_t offset)
{
    int i = 0, j;

    if (seg == NULL || seg->memory == NULL || seg->keymap == NULL || seg->keymap_used == 0)
        return -1;

    if (seg->keymap[0].ts >= offset) return 0;

    j = seg->keymap_used - 1;
    if (seg->keymap[j].ts < offset) return -1;

    for(i = 0; i < j; i ++) {
        if (seg->keymap[i].ts <= offset && offset < seg->keymap[i+1].ts) /* found match */
            break;
    }

    return i==j? -1:i;
}

/* switch to next stream buffer */
static void
stream_client(conn *c)
{
    int i, pos = -1;
    struct stream *s;
    uint32_t off_ts = 0;
    struct buffer *b, t;
    uint64_t ts;
    struct p2p_header p;

    if (c == NULL || c->s == NULL || c->fd <= 0) return;

    if (c->to_close) {
        /* M3U8 & TS clients go here */
        write_client(c);
        return;
    }

    s = c->s;
    if (c->stream_mode == P2P_MODE) {
        if (s->tail) {
            ts = (c->p2p_ts + 1) * 1000;
            off_ts = ts - s->start_ts;
            if (off_ts > s->tail->last_ts) {
                append_waitqueue(c);
                return;
            }
            b = find_p2p_buffer(s, ts);
            if (b) {
                memset(&p, 0, sizeof(p));
                p.magic[0] = 'P'; p.magic[1] = 'L';
                p.packet_number = ++ c->p2p_ts;
                p.length = sizeof(p) + b->used;

                parse_p2p_buffer(c, b);
                if (c->p2p_flag <= P2P_HAS_KEYFRAME && (c->p2p_duration > MINIMUM_P2P_DURATION)) {
                    /* duration > 0.95s & only have keyframe */
                    c->p2p_flag += P2P_IS_OK;
                }
                p.flag = c->p2p_flag;
                p.ver = 1;
                p.duration = c->p2p_duration;
                t.used = sizeof(p);
                t.size = t.used + 1;
                t.ptr = (void *) &p;
                buffer_append(c->w, &t);
                buffer_append(c->w, b);

                buffer_free(b);
                if (verbose_mode)
                    ERROR_LOG(conf.logfp, "client fd #%d(%s:%d) -> P2P at %.0f, flag 0x%x, duration %dms\n",
                        c->fd, c->remote_ip, c->remote_port, c->p2p_ts * 1.0, c->p2p_flag, (int) c->p2p_duration);
                if (c->blocks > 0) {
                    -- c->blocks;
                    if (c->blocks == 0) c->to_close = 1; /* close client after N blocks */
                }
            }
        }
    } else if (c->stream_mode == LIVE_MODE) {
        if (s->head == NULL || s->tail == NULL) {
            /* need more data from upstream */
            c->live = NULL;
            append_waitqueue(c);
            return;
        }

        if (c->live == NULL) {
            if ((c->seek_to_keyframe && s->tail->keymap_used == 0) ||
                (c->seek_to_keyframe == 0 && s->tail->map_used == 0)) {
                /* no keyframe or frame, wait for first frame */
                append_waitqueue(c);
                return;
            }

            if (s->last_stream_ts < c->offset) c->offset = s->last_stream_ts;

            /* set to use newest segment by default */
            c->live = s->tail;
            if (c->offset > 0) {
                off_ts = s->last_stream_ts - c->offset;
                /* seek to offset ms before now
                 * 1) offset_ts < s->head->first_ts
                 *  uses s->tail->last_ts
                 * 2) s->head->first_ts <= offset_ts <= s->head->last_ts
                 * 3) s->tail->first_ts <= offset_ts <= s->tail->last_ts
                 */
                if (s->head == s->tail) {
                    /* only one segment */
                    pos = find_keyframe(s->tail, off_ts);
                } else {
                    /* two segment */
                    if (off_ts <= s->head->last_ts) {
                        c->live = s->head;
                        pos = find_keyframe(s->head, off_ts);
                    } else /* if (s->tail->first_ts <= off_ts && off_ts <= s->tail->last_ts) */ {
                        pos = find_keyframe(s->tail, off_ts);
                    }
                }
                c->offset = 0;
            }

            /* set c->live_start */
            if (pos >= 0) {
                c->live_start = c->live->keymap[pos].start;
            } else if (c->seek_to_keyframe && c->live->keymap_used > 0) {
                c->live_start = c->live->keymap[c->live->keymap_used-1].start;
            } else if (c->live->map_used > 0) {
                c->live_start = c->live->map[c->live->map_used - 1].start;
            } else {
                c->live_start = 0;
            }

            /* set c->live_end */
            c->live_end = c->live->memory->used;
            c->live_end_ts = c->live->last_ts;
        } else if (c->live_start >= c->live_end) {
            /* previous write operation finished */
            if (c->live_end < c->live->memory->used) {
                /* current segment has more data to write*/
                c->live_start = c->live_end;
                if (c->live->is_full == 0 && c->live->map_used > 0) {
                    i = c->live->map_used - 1;
                    c->live_end = c->live->map[i].end;
                    c->live_end_ts = c->live->map[i].ts;
                } else {
                    c->live_end = c->live->memory->used; /* there may be some audio tag at the end of segment */
                    c->live_end_ts = c->live->last_ts;
                }
            } else {
                /* operation to write current segment completed
                 * try next segment now
                 */
                int to_rewind = 0;
                if (s->head && s->head != s->tail) {
                    /* make sure that we have two segments */
                    if (c->live->is_full == 1 || c->live == s->head) {
                        /* so c->live == s->head */
                        c->live = s->tail;
                        to_rewind = 1;
                    } else {
                        if (s->tail->memory->used < c->live_end) {
                            /* c->live == s->tail and s->tail->memory->used < c->live_end, it's strange.
                             * maybe client speed is too slow, live stream catchup the client with two rounds
                             */
                            c->live = s->tail;
                            to_rewind = 1;
                        } /* else, it's normal, just wait for more data */
                    }
                } /* else // only one segment, just wait for more data */

                if (to_rewind == 1) {
                    c->live_start = 0;
                    c->live_end = c->live->memory->used;
                    c->live_end_ts = c->live->last_ts;
                } else {
                    append_waitqueue(c);
                    return;
                }
            }
        }
    }

    write_client(c);
}

void
process_waitqueue(struct stream *s)
{
    conn *c;
    struct connection_list *l;
    int i, j;

    if (s == NULL || s->waitqueue == NULL || s->waitqueue->conns == NULL || s->waitqueue->used <= 0) return;

    /* switch waitqueue with waitqueue_bak */
    l = s->waitqueue;
    s->waitqueue = s->waitqueue_bak;
    s->waitqueue_bak = l;

    j = l->used;

    s->waitqueue->used = 0;
    s->waitqueue_bak->used = 0;

    for(i = 0; i < j; i ++) {
        c = l->conns[i];
        if (c) {
            c->in_waitqueue = 0;
            if (c->fd > 0) {
                stream_client(c);
            }
        }
    }
}

static void
http_handler(const int fd, const short which, void *arg)
{
    conn *c;
    int r;
    char buf[129];

    c = (conn *)arg;
    if (c == NULL) return;

    if (which & EV_READ) {
        switch(c->state) {
        case conn_read:
            r = read_buffer(fd, c->r);
            if (r <= 0) {
                if (r < 0) conn_close(c);
                return;
            }

            /* terminate it with '\0' for following strstr */
            c->r->ptr[c->r->used] = '\0';

            if (strstr(c->r->ptr, "\r\n\r\n")) {
                /* end of request http header */
                parse_request_line(c);
                if (c->is_crossdomainxml == 0)
                    handle_http_request(c);

                if (c->http_status == 0) {
                    c->state = conn_write;
                    /* write HTTP header and first part of stream to client */
                    r = write(c->fd, c->w->ptr, c->w->used);
                    if (r < 0) {
                        if (errno != EINTR && errno != EAGAIN) {
                            conn_close(c);
                            return;
                        }
                        r = 0;
                    }

                    c->wpos += r;
                    if (c->wpos == c->w->used) {
                        /* finish of writing header */
                        c->wpos = c->w->used = 0;
                        if (c->to_close) {
                            conn_close(c);
                            return;
                        }
                    }
                    stream_client(c);
                } else {
                    write_http_error(c);
                }
            }
            break;
        case conn_write: /* write live flash stream */
            /* just drop incoming unused bytes */
            r = read(c->fd, buf, 128);
            if ((r == -1 && errno != EINTR && errno != EAGAIN) || r == 0)
                conn_close(c);
            break;
        }
    } else if (which & EV_WRITE) {
        if (c->wpos < c->w->used || c->to_close == 1 || c->live)
            write_client(c);
        else
            append_waitqueue(c);
    }
}

static void
client_accept(const int fd, const short which, void *arg)
{
    conn *c = NULL;
    int newfd, i;
    struct sockaddr_in s_in;
    socklen_t len = sizeof(struct sockaddr_in);
    char *r = NULL;
    struct in_addr addr;

    memset(&s_in, 0, len);
    newfd = accept(fd, (struct sockaddr *) &s_in, &len);
    if (newfd < 0) {
        ERROR_LOG(conf.logfp, "FD #%d accept() failed\n", fd);
        return ;
    }

    for (i = 0; i < conf.maxfds; i ++) {
        c = client_conns + i;
        if (c->fd <= 0) break;
    }

    if (i == conf.maxfds) {
        write(newfd, "FULL OF CONNECTION\n", strlen("FULL OF CONNECTION\n"));
        close(newfd);
        return;
    }

    if (c->r == NULL) c->r = buffer_init(2048);
    if (c->w == NULL) c->w = buffer_init(2048);

    if (c->r == NULL || c->w == NULL) {
        write(newfd, "OUT OF MEMORY\n", strlen("OUT OF MEMORY\n"));
        close(newfd);
        return;
    }

    c->fd = newfd;
    c->state = conn_read;

    c->remote_ip[0] = '\0'; c->remote_port = 0;
    c->local_ip[0] = '\0'; c->local_port = 0;

    addr = s_in.sin_addr;
    r = inet_ntoa(addr);
    if (r == NULL) strcpy(c->remote_ip, "0.0.0.0");
    else strncpy(c->remote_ip, r, 19);

    c->remote_port = ntohs(s_in.sin_port);

    /* get local bind ip & port */
    len = sizeof(struct sockaddr_in);
    memset(&s_in, 0, len);
    if (getsockname(c->fd, (struct sockaddr *)&s_in, &len) == 0) {
        addr = s_in.sin_addr;
        if (NULL != (r = inet_ntoa(addr))) {
            strncpy(c->local_ip, r, 19);
            c->local_port = ntohs(s_in.sin_port);
        }
    }

    set_nonblock(c->fd);
    c->seek_to_keyframe = conf.seek_to_keyframe;

    c->is_writable = 0;
    event_base_set(main_base, &(c->ev));
    event_set(&(c->ev), c->fd, EV_READ|EV_PERSIST, http_handler, (void *) c);
    event_add(&(c->ev), 0);

    if (conf.client_expire_time > 0) c->expire_ts = cur_ts + conf.client_expire_time;
    else c->expire_ts = 0;

    if (verbose_mode) ERROR_LOG(conf.logfp, "NEW CLIENT FD #%d(%s:%d)\n", c->fd, c->remote_ip, c->remote_port);
}

static void
timer_service(const int fd, short which, void *arg)
{
    struct timeval tv;

    cur_ts = time(NULL);
    strftime(cur_ts_str, 127, "%Y-%m-%d %H:%M:%S", localtime(&cur_ts));

    tv.tv_sec = 1; tv.tv_usec = 0; /* check for every 1 seconds */
    event_add(&ev_timer, &tv);

    check_global_streams_waitqueue();
}

void
main_service(void)
{
    struct timeval tv;

    srand(getpid()); /* init random seed */

    event_set(&ev_master, mainfd, EV_READ|EV_PERSIST, client_accept, NULL);
    event_base_set(main_base, &ev_master);
    event_add(&ev_master, 0);

    /* timer */
    event_base_set(main_base, &ev_timer);
    evtimer_set(&ev_timer, timer_service, NULL);
    tv.tv_sec = 1; tv.tv_usec = 0; /* check for every 1 seconds */
    event_add(&ev_timer, &tv);

    /* event loop */
    event_base_dispatch(main_base);
}

void
close_all_clients(void)
{
    int i;

    for (i = 0; i < conf.maxfds; i ++) {
        if (client_conns[i].fd > 0) conn_close(client_conns + i);
    }
}

void
close_stream_clients(struct stream *s)
{
    int i;

    if (s == NULL) return;
    for (i = 0; i < conf.maxfds; i ++) {
        if (client_conns[i].fd > 0 && client_conns[i].s == s)
            conn_close(client_conns + i);
    }
}

/* to make sure all active client connections's stream is still exist */
void
update_all_client_conns(void)
{
    int i;
    struct stream *s;
    struct connection *c;

    for (i = 0; i < conf.maxfds; i ++) {
        c = client_conns + i;
        if (c->fd > 0) {
            s = find_stream(c->streamid, c->bps);
            if (s == NULL) {
                /* stream is not available, close client */
                conn_close(c);
            } else if (s != c->s) {
                /* stream object changed */
                if (c->live != NULL && c->live != s->tail && c->live != s->head) {
                    /* new global stream, client conns with old data */
                    conn_close(c);
                } else {
                    c->in_waitqueue = 0;
                    c->s = s;
                    stream_client(c);
                }
            } /* else stream unchanged */
        }
    }
}
