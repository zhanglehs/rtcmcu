/* supervisor related function and event handler
 * broadcast any change of live server settings to its clients
 */
#define _GNU_SOURCE
#include <sys/types.h>
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
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>

#include "transporter.h"

struct admin_connection
{
    buffer *r, *w;
    int wpos;

    int fd;
    struct event ev;

    /* connection state
     * 0 -> not connected, 1 -> connected
     * 2 -> read, 3 -> write
     */
    int state;
    int is_sibling;
};

struct admin_connection *admin_connections;
int max_admin_connections = 512; /* max 512 siblings, should be enough */
static struct event ev_admin_timer;
static struct event ev_live_stream_timer;

struct backends
{
    struct stream **s;
    int used, size;
};

struct backends global_backends;

/* transporter.c */
extern struct config conf;
extern struct event_base *main_base;
extern int verbose_mode;
extern char cur_ts_str[128];

/* backend.c */
extern int connect_live_stream_server(struct stream *);
extern void close_live_stream(struct stream *);
extern void reset_stream_ts_data(struct stream *);

/* buffer.c */
extern void free_segment(struct segment *);

/* client.c */
extern int allow_live_service;
extern void close_all_clients(void);
extern void close_stream_clients(struct stream *);
extern void conn_close(conn *);
extern void update_all_client_conns(void);

static void supervisor_handler(const int, const short, void *);

static const char resivion[] __attribute__((used)) = { "$Id: control.c 512 2011-03-22 07:51:37Z qhy $" };

#define MAX_BACKENDS_COUNT 100
int
init_global_backends(void)
{
    global_backends.s = (struct stream **) calloc(sizeof(struct stream *), MAX_BACKENDS_COUNT);
    if (global_backends.s == NULL) {
        ERROR_LOG(conf.logfp, "OUT OF MEMORY FOR INIT_GLOBAL_BACKENDS\n");
        return 1;
    }
    global_backends.size = MAX_BACKENDS_COUNT;
    global_backends.used = 0;
    return 0;
}
#undef MAX_BACKENDS_COUNT

/* free stream data */
static void
free_stream(struct stream *s)
{
    if (s == NULL) return;
    close_live_stream(s);

    buffer_free(s->onmetadata);
    buffer_free(s->avc0);
    buffer_free(s->aac0);
    buffer_free(s->r);
    buffer_free(s->w);
    if (s->head == s->tail)
        s->head = NULL;
    free_segment(s->tail);
    free_segment(s->head);

    reset_stream_ts_data(s);
    buffer_free(s->ipad_mem);

    if (s->waitqueue) {
        free(s->waitqueue->conns);
        free(s->waitqueue);
    }
    if (s->waitqueue_bak) {
        free(s->waitqueue_bak->conns);
        free(s->waitqueue_bak);
    }
    free(s->g_param.sps);
    free(s->g_param.pps);
    free(s);
}

void
free_global_backends(void)
{
    int i;
    struct stream *s;

    for (i = 0; i < global_backends.used; i ++) {
        s = global_backends.s[i];
        if (s) {
            free_stream(s);
            global_backends.s[i] = NULL;
        }
    }

    free(global_backends.s);
    global_backends.s = NULL;
    global_backends.used = global_backends.size = 0;
}

struct stream *
find_stream(int streamid, int bps)
{
    int i;
    struct stream *s = NULL;

    for (i = 0; i < global_backends.used; i ++) {
        s = global_backends.s[i];
        if (s && s->si.streamid == streamid && s->si.bps == bps)
            return s;
    }

    return NULL;
}

void
check_global_streams_waitqueue(void)
{
    int i, j, k, changed = 0;
    struct stream *s;
    conn *c;
    struct connection_list *l;

    for (i = 0; i < global_backends.used; i ++) {
        s = global_backends.s[i];
        changed = 0;

        if (s && s->ts_reset_ticker > 0)
            -- s->ts_reset_ticker;

        if (s && s->waitqueue && s->waitqueue->conns && s->waitqueue->used > 0) {
            j = s->waitqueue->used;
            for (k = 0; k < j; k ++) {
                c = s->waitqueue->conns[k];
                if (c == NULL) {
                    changed = 1;
                } else if (c->fd == 0 || (c->fd > 0 && conf.client_expire_time > 0 && c->expire_ts < cur_ts)) {
                    /* close expired client connection */
                    if (c->fd > 0) {
                        if (verbose_mode)
                            ERROR_LOG(conf.logfp, "closing expired client fd #%d(%s:%d)\n", c->fd, c->remote_ip, c->remote_port);
                        conn_close(c);
                    }
                    s->waitqueue->conns[k] = NULL;
                    changed = 1;
                }
            }

            if (changed == 1) {
                /* remove empty connection from wait queue */
                s->waitqueue_bak->used = 0;
                for (i = 0; i < s->waitqueue->used; i ++) {
                    c = s->waitqueue->conns[i];
                    if (c) {
                        s->waitqueue_bak->conns[s->waitqueue_bak->used ++] = c;
                        s->waitqueue->conns[i] = NULL;
                    }
                }
                s->waitqueue->used = 0;

                /* switch waitqueue with waitqueue_bak */
                l = s->waitqueue;
                s->waitqueue = s->waitqueue_bak;
                s->waitqueue_bak = l;
            }
        }
    }
}

void
remove_live_stream_timer(void)
{
    event_del(&ev_live_stream_timer);
}

static buffer *global_supervisor_data = NULL;

static void
setup_supervisor_timer(void)
{
    struct timeval tv;

    if (global_backends.used == 0)
        tv.tv_sec = 3; /* try to connect supervisor server every 3 seconds if no stream*/
    else
        tv.tv_sec = 15; /* try to connect supervisor server every 30 seconds */
    tv.tv_usec = 0;
    event_add(&ev_admin_timer, &tv);
}

static void
close_admin_connection(struct admin_connection *ac)
{
    if (ac == NULL) return;
    if (ac->fd > 0) {
        event_del(&(ac->ev));
        close(ac->fd);
        ac->fd = 0;
    }
    ac->state = 0;
    if (ac->is_sibling)
        setup_supervisor_timer();
}

static void
broadcast_supervisor_cmd(buffer *b, int len)
{
    int i, r;
    struct admin_connection *ac;

    if (b == NULL || b->used == 0 || len == 0) return;
    for (i = 2; i < max_admin_connections; i ++) {
        ac = admin_connections + i;
        if (ac->fd > 0) {
            buffer_copy(ac->w, b);
            if (len < b->used) ac->w->used = len;
            ac->wpos = 0;
            r = write(ac->fd, ac->w->ptr, ac->w->used);
            if (r < 0) {
                if (errno != EINTR && errno != EAGAIN) {
                    close_admin_connection(ac);
                    break;
                }
                r = 0;
            }

            ac->wpos += r;
            event_del(&ac->ev);
            if (ac->wpos == ac->w->used) {
                ac->wpos = 0;
                ac->w->used = 0;
                event_set(&ac->ev, ac->fd, EV_READ|EV_PERSIST, supervisor_handler, (void *)ac);
                event_add(&ac->ev, 0);
            } else {
                event_set(&ac->ev, ac->fd, EV_WRITE|EV_PERSIST, supervisor_handler, (void *)ac);
                event_add(&ac->ev, 0);
            }
        }
    }
}

#define LIVE_STREAM_INTERVAL 3

static void
live_stream_timer_service(const int fd, short which, void *arg)
{
    int i;
    struct stream *s;
    struct timeval tv;

    for (i = 0; i < global_backends.used; i ++) {
        s = global_backends.s[i];
        /* retry stream connection if there has waited connections or retry count is below setting */
        if (s && s->connected == 0 &&
            (conf.max_backend_retry_count == 0 || s->waitqueue->used > 0 || s->retry_count < conf.max_backend_retry_count)) /* 40 * 3 = 120 second */
            connect_live_stream_server(s);
    }

    tv.tv_sec = LIVE_STREAM_INTERVAL; tv.tv_usec = 0;
    event_add(&ev_live_stream_timer, &tv);
}

static void
prepare_supervisor_data(buffer *b)
{
    struct proto_header header;
    struct stream_info si;
    int len = 0, i, active_count = 0;
    struct sockaddr_in ip;
    struct stream *s;

    if (b == NULL || b->ptr == NULL) return;

    for (i = 0; i < global_backends.used; i ++) {
        if (global_backends.s[i] && global_backends.s[i]->si.max_depth > 1) ++ active_count;
    }

    strncpy(header.magic, PROTO_MAGIC, 2);
    len = sizeof(struct proto_header) + active_count * sizeof(struct stream_info);
    if (b->size < len) buffer_expand(b, len + 1);
    header.length = htons(len);
    header.method = LIST_ALL_CMD;
    header.reserved = 0;
    header.stream_count = htons(active_count);
    memcpy(b->ptr, &header, sizeof(header));

    len = sizeof(header);
    for (i = 0; active_count > 0 && i < global_backends.used; i ++) {
        s = global_backends.s[i];
        if (s == NULL || s->si.max_depth <= 1) continue;
        if (verbose_mode)
            ERROR_LOG(conf.logfp, "stream #%d/#%d: depth -> #%d\n", s->si.streamid, s->si.bps, s->si.max_depth);

        si.uuid = s->si.uuid;
        si.streamid = htons(s->si.streamid);
        si.bps = htons(s->si.bps);
        si.port = htons(conf.port);
        if (conf.stream_ip[0] == '\0') ip.sin_addr.s_addr = inet_addr(conf.admin_ip);
        else ip.sin_addr.s_addr = inet_addr(conf.stream_ip);
        memcpy(&(si.ip), &(ip.sin_addr.s_addr), sizeof(si.ip));
        si.ip = htonl(si.ip);

        si.buffer_minutes = htons(s->si.buffer_minutes);

        si.max_ts_count = s->si.max_ts_count;
        si.max_depth = s->si.max_depth - 1;
        si.reserved_3 = s->si.reserved_3; si.reserved_4 = s->si.reserved_4;

        memcpy(b->ptr + len, &si, sizeof(si));
        len += sizeof(si);
    }
    b->used = len;
}

static struct stream *
new_stream_data(void)
{
    struct stream *s;

    s = (struct stream *) calloc(sizeof(struct stream), 1);
    if (s) {
        s->waitqueue = (struct connection_list *)calloc(sizeof(struct connection_list), 1);
        if (s->waitqueue) {
            s->waitqueue->conns = (struct connection **)calloc(sizeof(struct connection *), conf.maxfds);
            if (s->waitqueue->conns) {
                s->waitqueue->size = conf.maxfds;
                s->waitqueue->used = 0;
            } else {
                free(s->waitqueue);
                free(s);
                return NULL;
            }
        } else {
            free(s);
            return NULL;
        }

        s->waitqueue_bak = (struct connection_list *)calloc(sizeof(struct connection_list), 1);
        if (s->waitqueue_bak) {
            s->waitqueue_bak->conns = (struct connection **)calloc(sizeof(struct connection *), conf.maxfds);
            if (s->waitqueue_bak->conns) {
                s->waitqueue_bak->size = conf.maxfds;
                s->waitqueue_bak->used = 0;
            } else {
                free(s->waitqueue->conns);
                free(s->waitqueue);
                free(s->waitqueue_bak);
                free(s);
                return NULL;
            }
        } else {
            free(s->waitqueue->conns);
            free(s->waitqueue);
            free(s);
            return NULL;
        }
    }
    return s;
}

static void
reset_all_streams(void)
{
    int i;
    struct stream *s;

    /* clear all global streams */
    close_all_clients();
    for (i = 0; i < global_backends.size; i ++) {
        s = global_backends.s[i];
        if (s) {
            free_stream(s);
            global_backends.s[i] = NULL;
        }
    }
    global_backends.used = 0;
}

static void
parse_supervisor_command(struct admin_connection *ac)
{
    int stream_count, i, j, len, buflen, empty_slot = 0;
    struct proto_header *header;
    struct stream_info *si;
    struct stream *s;
    uint64_t uuid;

    if (ac == NULL || ac->r->used < sizeof(struct proto_header)) return;

    header = (struct proto_header *) ac->r->ptr;
    if (strncmp(header->magic, PROTO_MAGIC, 2)) {
        /* wrong MAGIC */
        ac->r->used = 0;
        return;
    }

    buflen = sizeof(struct proto_header);
    switch(header->method) {
    case STOP_ALL_CMD:
        if (verbose_mode)
            ERROR_LOG(conf.logfp, "RECEIVE STOP_ALL_CMD PACKET\n");
        allow_live_service = 0;
        close_all_clients();
        /* write received supervisor data to siblings */
        broadcast_supervisor_cmd(ac->r, sizeof(struct proto_header));
        break;
    case START_ALL_CMD:
        if (verbose_mode)
            ERROR_LOG(conf.logfp, "RECEIVE START_ALL_CMD PACKET\n");
        allow_live_service = 1;
        /* try to connect to live stream server */
        for (i = 0; i < global_backends.used; i ++) {
            s = global_backends.s[i];
            if (s && s->connected == 0) {
                s->retry_count = 0;
                connect_live_stream_server(s);
            }
        }

        /* write received supervisor data to siblings */
        broadcast_supervisor_cmd(ac->r, sizeof(struct proto_header));
        break;
    case CLEAR_ALL_CMD:
        if (verbose_mode)
            ERROR_LOG(conf.logfp, "RECEIVE CLEAR_ALL_CMD PACKET\n");
        reset_all_streams();
        /* write received supervisor data to it's supervisor clients */
        broadcast_supervisor_cmd(ac->r, sizeof(struct proto_header));
        /* close connection to remote supervisor server
         * then transporter will connect to remote supervisor soon
         * and get the new streams settings
         */
        close_admin_connection(admin_connections + 1);
        break;
    case RESET_CMD:
        if (verbose_mode)
            ERROR_LOG(conf.logfp, "RECEIVE RESET_CMD #%d PACKET\n", header->reserved);
        for (i = 0; i < global_backends.used; i ++) {
            s = global_backends.s[i];
            if (s == NULL) continue;
            uuid = ntohl(s->si.uuid);
            if (uuid == header->reserved) {
                close_stream_clients(s); /* close connected clients for this stream */
                close_live_stream(s);
                buffer_free(s->onmetadata);
                buffer_free(s->avc0);
                buffer_free(s->aac0);
                s->avc0 = s->aac0 = s->onmetadata = NULL;
                
                if (s->r) s->r->used = 0;
                if (s->w) s->w->used = 0;
                
                if (s->head == s->tail) s->head = NULL;
                free_segment(s->tail); free_segment(s->head);
                s->head = s->tail = NULL;
                
                reset_stream_ts_data(s);

                if (s->waitqueue) s->waitqueue->used = 0;
                if (s->waitqueue_bak) s->waitqueue_bak->used = 0;
                s->retry_count = 0;
                s->start_ts = 0;
                s->last_stream_ts = 0;
                break;
            }
        }
        /* write received supervisor data to siblings */
        broadcast_supervisor_cmd(ac->r, sizeof(struct proto_header));
        break;
    case LIST_ALL_CMD:
        len = ntohs(header->length);
        if (len > ac->r->used) {
            /* not enough data */
            buflen = 0;
            break;
        }
        buflen = len;
        stream_count = ntohs(header->stream_count);

        if (stream_count == 0) {
            /* == CLEAR_ALL_CMD */
            if (verbose_mode)
                ERROR_LOG(conf.logfp, "0 STREAM LIST_ALL PACKET == CLEAR_ALL_CMD\n");
            reset_all_streams();
            /* write received supervisor data to it's supervisor clients */
            broadcast_supervisor_cmd(ac->r, sizeof(struct proto_header));
            break;
        } else if ((stream_count * sizeof(struct stream_info) != (len - sizeof(struct proto_header)))) {
            if (verbose_mode)
                ERROR_LOG(conf.logfp, "RECEIVE BAD LISTALL PACKET\n");
            break;
        }

        /* make sure that we have enough data */
        if ((stream_count + global_backends.used) > global_backends.size) { /* to increase global_backends.size */
            void *p;

            p = realloc(global_backends.s, sizeof(struct stream *) * (stream_count + global_backends.used));
            if (p) {
                global_backends.s = p;
                global_backends.size += 10;
            } else {
                if (verbose_mode)
                    ERROR_LOG(conf.logfp, "OUT OF MEMORY WHILE EXPANDING STREAM STRUCTURE\n");
            }
        }

        si = (struct stream_info *) (ac->r->ptr + sizeof(struct proto_header));
        for (i = 0; i < stream_count; i ++) {
            si[i].streamid = ntohs(si[i].streamid);
            si[i].bps = ntohs(si[i].bps);
            si[i].port = ntohs(si[i].port);
            si[i].ip = ntohl(si[i].ip);
            si[i].buffer_minutes = ntohs(si[i].buffer_minutes);
        }

        if (global_backends.used == 0) {
            /* previous global backends is zero */
            for (i = 0; i < stream_count && global_backends.used < global_backends.size; i ++) {
                s = new_stream_data();
                if (s) {
                    s->si = si[i];

                    global_backends.s[global_backends.used ++] = s;
                    if (global_backends.used >= global_backends.size) {
                        if (verbose_mode)
                            ERROR_LOG(conf.logfp, "STREAM COUNT %d > SIZE %d\n", stream_count, global_backends.size);
                        break;
                    }
                } else {
                    if (verbose_mode)
                        ERROR_LOG(conf.logfp, "OUT OF MEMORY WHILE PARSING LIST_ALL PACKET\n");
                }
            }
        } else {
            /* we have old global stream data */
            for (i = 0; i < global_backends.used; i ++) {
                s = global_backends.s[i];
                if (s == NULL) continue;

                for (j = 0; j < stream_count; j ++) {
                    if (si[j].bps == s->si.bps && si[j].streamid == s->si.streamid) /* found matched */
                        break;
                }

                if (j == stream_count) {
                    /* not found */
                    if (verbose_mode)
                        ERROR_LOG(conf.logfp, "#%d: remove #%d/#%d's live stream\n", i+1, s->si.streamid, s->si.bps);
                    close_stream_clients(s); /* close connected clients for this stream */
                    free_stream(s);
                    global_backends.s[i] = NULL;
                } else {
                    if (si[j].port != s->si.port || si[j].ip != s->si.ip) {
                        /* ip/port changed, we need to close previous live stream connection */

                        /* transporter will siwtch stream at keyframe if connected*/
                        if (s->connected == 1) s->switch_backend = 1;
                        /* close_live_stream(s); */
                    }
                    s->si = si[j];
                    si[j].bps = si[j].streamid = 0;
                }
            }

            /* add new global stream */
            for (j = 0; j < stream_count && global_backends.used < global_backends.size; j ++) {
                if (si[j].bps == 0 || si[j].streamid == 0) continue; /* already processed */
                s = new_stream_data();
                if (s == NULL) {
                    if (verbose_mode)
                        ERROR_LOG(conf.logfp, "OUT OF MEMORY WHILE PARSING LIST_ALL PACKET\n");
                    continue;
                }

                s->si = si[j];
                /* to find empty slot in global_backends */
                for (i = 0; i < global_backends.used; i ++) {
                    if (global_backends.s[i] == NULL) break;
                }

                if (i == global_backends.used) {
                    global_backends.s[global_backends.used ++] = s;
                } else {
                    global_backends.s[i] = s;
                }
            }

            for (i = 0; i < global_backends.used; i ++) {
                /* remove unused slots */
                if (global_backends.s[i] == NULL) {
                    for (j = i + 1; j < global_backends.used; j ++) {
                        if (global_backends.s[j]) {
                            global_backends.s[i] = global_backends.s[j];
                            global_backends.s[j] = NULL;
                            break;
                        }
                    }

                    if (j == global_backends.used) empty_slot ++;
                }
            }

            global_backends.used -= empty_slot;
        }

        /* connect to live stream server */
        for (i = 0; i < global_backends.used; i ++) {
            s = global_backends.s[i];
            if (s == NULL) continue;
            if (s->si.buffer_minutes > 0)
                s->live_buffer_time = s->si.buffer_minutes * 60;
            else
                s->live_buffer_time = conf.live_buffer_time;

            if ((s->max_ts_count > 0) && (s->max_ts_count != s->si.max_ts_count)) {
                /* max_ts_count changed, reset all ts structure */
                reset_stream_ts_data(s);
            }
            s->max_ts_count = s->si.max_ts_count;

            if (verbose_mode)
                ERROR_LOG(conf.logfp, "stream #%d/#%d: buffer time is %d second, max_ts_count is %d\n",
                        s->si.streamid, s->si.bps, s->live_buffer_time, s->max_ts_count);

            if (s->switch_backend == 0) connect_live_stream_server(s);
        }

        prepare_supervisor_data(global_supervisor_data);
        broadcast_supervisor_cmd(global_supervisor_data, global_supervisor_data->used);
        update_all_client_conns();
        if (verbose_mode)
            ERROR_LOG(conf.logfp, "LIST_ALL_CMD PACKET -> #%d STREAMS\n", global_backends.used);
        break;
    default:
        /* bad supervisor packet */
        buflen = ac->r->used;
        break;
    }

    if (ac->r->used <= buflen) {
        ac->r->used = 0;
    } else if (buflen > 0) {
        memmove(ac->r->ptr, ac->r->ptr + buflen, ac->r->used - buflen);
        ac->r->used -= buflen;
    }
}

static void
supervisor_handler(const int fd, const short which, void *arg)
{
    struct admin_connection *ac;
    int toread = -1, r, socket_error, changed = 0;
    socklen_t socket_error_len;

    if (arg == NULL) return;
    ac = (struct admin_connection *)arg;

    if (which & EV_READ) {
        r = read_buffer(fd, ac->r);
        if (r < 0) {
            close_admin_connection(ac);
        } else if (r > 0){
            parse_supervisor_command(ac);
        }
    } else if (which & EV_WRITE) {
        if (ac->state == 0) {
            /* try to connect to remote supervisor server */
            socket_error_len = sizeof(socket_error);
            socket_error = 0;
            /* try to finish the connect() */
            if (0 != getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_len) || socket_error != 0) {
                close_admin_connection(ac);
            } else {
                ac->state = 1;
                ac->wpos = ac->r->used = 0;
                changed = 1;
            }
        } else if (ac->w->used > ac->wpos) {
            toread = ac->w->used - ac->wpos;
            r = write(fd, ac->w->ptr + ac->wpos, toread);
            if (r < 0) {
                if (errno != EINTR && errno != EAGAIN) {
                    close_admin_connection(ac);
                    return;
                }
                r = 0;
            }

            ac->wpos += r;
            if (ac->wpos == ac->w->used) {
                ac->wpos = ac->w->used = 0;
                changed = 1;
            }
        } else {
            ac->wpos = ac->w->used = 0;
            changed = 1;
        }

        if (changed == 1) {
            event_del(&ac->ev);
            event_set(&ac->ev, ac->fd, EV_READ|EV_PERSIST, supervisor_handler, (void *) ac);
            event_add(&ac->ev, 0);
        }
    }
}

static void
admin_client_accept(const int fd, const short which, void *arg)
{
    int newfd, r, i;
    struct admin_connection *ac = NULL;
    struct sockaddr_in s_in;
    socklen_t len = sizeof(struct sockaddr_in);

    memset(&s_in, 0, len);
    newfd = accept(fd, (struct sockaddr *) &s_in, &len);
    if (newfd < 0) {
        ERROR_LOG(conf.logfp, "fd #%d accept() failed\n", fd);
        return ;
    }

    /* slot 0, slot 1 are reserved */
    for (i = 2; i < max_admin_connections; i ++) {
        ac = admin_connections + i;
        if (ac->fd <= 0) break;
    }

    if (i == max_admin_connections) {
        close(newfd);
        ERROR_LOG(conf.logfp, "fd #%d: out of connection\n", newfd);
        return;
    }

    set_nonblock(newfd);
    ac->fd = newfd;
    if (ac->r == NULL) ac->r = buffer_init(2048);
    if (ac->w == NULL) ac->w = buffer_init(2048);
    if (ac->r == NULL || ac->w == NULL) {
        ERROR_LOG(conf.logfp, "fd #%d accept failed because of out of memory\n", newfd);
        return ;
    }

    ac->r->used = ac->w->used = 0;
    ac->wpos = 0;

    ac->state = 1; /* connected */
    buffer_copy(ac->w, global_supervisor_data);
    if (ac->w->used > 0) {
        r = write(ac->fd, ac->w->ptr, ac->w->used);
        if (r == -1) {
            if (errno != EAGAIN && errno != EINTR) {
                /* write failed */
                close(ac->fd);
                ac->fd = 0;
                ac->state = 0;
                return;
            }
            r = 0;
        }

        ac->wpos = r;
        if (ac->wpos < ac->w->used)
            event_set(&ac->ev, ac->fd, EV_WRITE|EV_PERSIST, supervisor_handler, (void *) ac);
        else {
            ac->wpos = ac->w->used = 0;
            event_set(&ac->ev, ac->fd, EV_READ|EV_PERSIST, supervisor_handler, (void *) ac);
        }
    } else {
        event_set(&ac->ev, ac->fd, EV_READ|EV_PERSIST, supervisor_handler, (void *) ac);
    }
    event_add(&ac->ev, 0);
}

static void
connect_supervisor_server(void)
{
    int r;
    socklen_t len;
    struct admin_connection *ac = NULL;

    ac = admin_connections + 1; /* second global admin_connection structure */
    ac->is_sibling = 1;

    if (ac->fd > 0) {
        ERROR_LOG(conf.logfp, "previous fd #%d is pending\n", ac->fd);
        return;
    }

    ac->state = 0;
    ac->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ac->fd < 0) {
        ERROR_LOG(conf.logfp, "can't create tcp socket\n");
        setup_supervisor_timer();
        return;
    }

    if (ac->r == NULL) ac->r = buffer_init(2048);
    if (ac->w == NULL) ac->w = buffer_init(2048);
    ac->wpos = 0;

    if (ac->r == NULL || ac->w == NULL) {
        ERROR_LOG(conf.logfp, "fd #%d can't alloc memory\n", ac->fd);
        close(ac->fd);
        setup_supervisor_timer();
        return;
    }

    if (verbose_mode)
        ERROR_LOG(conf.logfp, "connecting to remote admin server(%s:%d)\n",
                   inet_ntoa(conf.supervisor_server.sin_addr), ntohs(conf.supervisor_server.sin_port));

    set_nonblock(ac->fd);
    len = sizeof(struct sockaddr);
    r = connect(ac->fd, (struct sockaddr *) &conf.supervisor_server, len);

    if (r == -1 && errno != EINPROGRESS && errno != EALREADY) {
        /* connect failed */
        close(ac->fd);
        ac->fd = 0;
        setup_supervisor_timer();
    } else {
        ac->wpos = ac->w->used = 0;

        if (r == -1)
            event_set(&ac->ev, ac->fd, EV_WRITE|EV_PERSIST, supervisor_handler, (void *)ac);
        else { /* r == 0, connection success */
            ac->state = 1;
            if (verbose_mode)
                ERROR_LOG(conf.logfp, "fd #%d supervisor server OK\n", ac->fd);
            event_set(&ac->ev, ac->fd, EV_READ|EV_PERSIST, supervisor_handler, (void *)ac);
        }
        event_base_set(main_base, &ac->ev);
        event_add(&ac->ev, 0);
    }
}

static void
supervios_timer_service(const int fd, short which, void *arg)
{
    connect_supervisor_server();
}

int
setup_control_server(void)
{
    struct admin_connection *ac;
    struct sockaddr_in server;
    struct timeval tv;

    /* allocated global control connection structure */
    admin_connections = (struct admin_connection *)calloc(sizeof(struct admin_connection), max_admin_connections);
    if (admin_connections == NULL) {
        ERROR_LOG(conf.logfp, "out of memory for control server\n");
        return 1;
    }

    global_supervisor_data = buffer_init(1024);
    if (global_supervisor_data == NULL) {
        ERROR_LOG(conf.logfp, "out of memory for control server\n");
        return 1;
    }

    ac = admin_connections + 0; /* first global admin_connection structure */
    ac->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ac->fd < 0) {
        ERROR_LOG(conf.logfp, "can't create socket for control server\n");
        return 1;
    }

    set_nonblock(ac->fd);

    memset((char *) &server, 0, sizeof(server));
    server.sin_port = htons(conf.admin_port);
    server.sin_family = AF_INET;
    if (conf.admin_ip[0] == '\0')
        server.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        server.sin_addr.s_addr = inet_addr(conf.admin_ip);

    if (bind(ac->fd, (struct sockaddr *) &server, sizeof(server)) || listen(ac->fd, 512)) {
        ERROR_LOG(conf.logfp, "fd #%d can't bind/listen: %s\n", ac->fd, strerror(errno));
        close(ac->fd);
        return 1;
    }

    if (ac->r == NULL) ac->r = buffer_init(2048);
    if (ac->w == NULL) ac->w = buffer_init(2048);
    ac->wpos = 0;
    if (ac->r == NULL || ac->w == NULL) {
        ERROR_LOG(conf.logfp, "fd #%d can't alloc memory\n", ac->fd);
        close(ac->fd);
        return 1;
    }

    event_set(&ac->ev, ac->fd, EV_READ|EV_PERSIST, admin_client_accept, NULL);
    event_base_set(main_base, &ac->ev);
    event_add(&ac->ev, 0);

    /* setup admin timer */
    event_base_set(main_base, &ev_admin_timer);
    evtimer_set(&ev_admin_timer, supervios_timer_service, NULL);

    /* setup live stream timer */
    event_base_set(main_base, &ev_live_stream_timer);
    evtimer_set(&ev_live_stream_timer, live_stream_timer_service, NULL);
    tv.tv_sec = LIVE_STREAM_INTERVAL; tv.tv_usec = 0;
    event_add(&ev_live_stream_timer, &tv);

    connect_supervisor_server();
    return 0;
}
