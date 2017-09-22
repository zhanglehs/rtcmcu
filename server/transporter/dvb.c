/* DVB-C UDP/TCP Receiver
 *
 * <config>
    <udp port='xx' path='/uri' />
    <tcp port='xx' />
    <http port='xx' />
 * </config>
 *
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
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <sys/uio.h>
#include <signal.h>

#include "xml.h"
#include "buffer.h"

struct dvb
{
    struct event ev;
    int port;
    int fd;

    buffer *uri;

    buffer *head, *tail;

    struct dvb_client *clients;
    int tail_pos;

    int buffer_length;

    int is_udp;

    int tcp_state;

    struct dvb *next;
};

struct dvb_client
{
    int fd, state;
    int http_status;

    struct event ev;
    struct dvb_client *next;

    buffer *r, *w;
    int wpos;

    int wait;
    struct dvb *d;

    buffer *live;
    int live_start, live_end;

    char remote_ip[20];
};

static int http_fd = 0, http_port = 80, verbose_mode = 0, tcp_fd = 0, tcp_port = 0;

struct dvb *dvb_udp = NULL, *dvb_tcp = NULL;

time_t cur_ts;
char cur_ts_str[128];

static struct event ev_http, ev_receiver_timer, ev_tcp;

static void dvb_http_handler(const int, const short, void *);
static void disable_dvb_client_write(struct dvb_client *);
static void enable_dvb_client_write(struct dvb_client *);
static void write_dvb_client(struct dvb_client *);

static const char resivion[] __attribute__((used)) = { "$Id: dvb.c 434 2010-12-13 02:56:01Z qhy $" };

static void
show_dvb_help(void)
{
    fprintf(stderr, "Youku DVB UDP Receiver v%s, Build-Date: " __DATE__ " " __TIME__ "\n"
          "Usage:\n"
          " -h this message\n"
          " -c file, config file, default is /etc/flash/dvb.xml\n"
          " -D don't go to background\n"
          " -v verbose\n\n", VERSION);
}

static int
parse_dvbconf(char *file)
{
    struct xmlnode *mainnode, *config, *p;
    char *q;
    int r, i, len;
    struct dvb *d = NULL;

    if (file == NULL || file[0] == '\0') return 0;

    mainnode = xmlloadfile(file);
    if (mainnode == NULL) return 1;

    config = xmlgetchild(mainnode, "config", 0);

    /*
    <udp port='xx' path='/uri' />
    <http port='xx' />
    */
    if (config) {
        /* <http port='80' /> */
        p = xmlgetchild(config, "http", 0);
        if (p) {
            q = xmlgetattr(p, "port");
            if (q) {
                http_port = atoi(q);
                if (http_port <= 0) http_port = 80;
            }
        }

        /* <tcp port='xxx' /> */
        p = xmlgetchild(config, "tcp", 0);
        if (p) {
            q = xmlgetattr(p, "port");
            if (q) {
                tcp_port = atoi(q);
                if (tcp_port < 0) tcp_port = 0;
            }
        }

        /* udp */
        r = xmlnchild(config, "udp");
        if (r == 0) {
            freexml(mainnode);
            if (tcp_port == 0) return 1;
            else return 0;
        }

        /* <udp port='xx' url='/uri' /> */
        for (i = 0; i < r; i ++) {
            p = xmlgetchild(config, "udp", i);
            if (p == NULL) continue;
            d = (struct dvb *) calloc(sizeof(*d), 1);
            if (d == NULL) continue;
            q = xmlgetattr(p, "port");
            if (q) {
                d->port = atoi(q);
                if (d->port < 0) d->port = 0;
            }

            q = xmlgetattr(p, "buffer");
            if (q) {
                len = atoi(q);
                if (len <= 0) len = 32;
            } else {
                len = 32;
            }
            d->buffer_length = len * 1024 * 1024;

            d->tail_pos = 0;
            d->head = NULL;
            d->tail = NULL;
            d->clients = NULL;
            d->is_udp = 1;
            d->next = NULL;

            q = xmlgetattr(p, "url");
            if (q) {
                len = strlen(q);
                d->uri = buffer_init(len + 1);
                if (d->uri) {
                    strcpy(d->uri->ptr, q);
                    d->uri->used = len;
                    d->next = dvb_udp;
                    dvb_udp = d;
                    d = NULL;
                }
            }

            if (d) {
                free(d);
                d = NULL;
            }
        }
    }

    freexml(mainnode);

    return 0;
}

static void
stream_dvb_client(struct dvb_client *c)
{
    struct dvb *d;
    int r = 0;

    if (c == NULL || c->d == NULL) return;

    d = c->d;

    if (c->live == NULL) {
        if (d->tail == NULL) {
            r = 1;
        } else {
            c->live = d->tail;
            c->live_start = c->live_end = c->live->used; /* don't cached MPEG TS buffer for new request */
        }
    } else {
        if (c->live == d->tail) {
            if (d->tail->used <= c->live_end)
                r = 1;
            else
                c->live_end = d->tail->used;
        } else {
            if (c->live->used <= c->live_start) {
                /* switch to new head */
                c->live = d->tail;
                c->live_start = 0;
                c->live_end = c->live->used;
            } else {
                /* finish oldest segment */
                c->live_end = c->live->used;
            }
        }
    }

    if (r) {
        disable_dvb_client_write(c);
    } else {
        enable_dvb_client_write(c);
        write_dvb_client(c);
    }
}

#define MAXLEN 4096

static void
dvb_udp_handler(const int fd, const short which, void *arg)
{
    struct dvb *d;
    struct sockaddr_in server;
    int r;
    buffer *b;
    socklen_t structlength;
    struct dvb_client *c;

    d = (struct dvb *)arg;
    if (d == NULL) exit(1);

    if (d->tail == NULL) {
        d->tail = buffer_init(d->buffer_length);

        if (d->tail == NULL) {
            fprintf(stderr, "%s: (%s.%d) OUT OF MEMORY FOR UDP RECEIVER\n", cur_ts_str, __FILE__, __LINE__);
            return;
        }

        if (verbose_mode)
            fprintf(stderr, "%s: (%s.%d) #1 %dMB SEGMENT FOR PORT #%d(%s)\n",
                    cur_ts_str, __FILE__, __LINE__, d->buffer_length >> 20, d->port, d->uri->ptr);
    }

    if ((d->tail->used + MAXLEN) > d->tail->size) {
        /* we need to switch to new buffer */
        if (d->head == NULL) {
            b = buffer_init(d->buffer_length);
            if (b == NULL) {
                fprintf(stderr, "%s: (%s.%d) OUT OF MEMORY FOR UDP RECEIVER\n", cur_ts_str, __FILE__, __LINE__);
                return;
            }
            d->head = d->tail;
            d->tail = b;
            if (verbose_mode)
                fprintf(stderr, "%s: (%s.%d) #2 %dMB SEGMENT FOR PORT #%d(%s)\n",
                        cur_ts_str, __FILE__, __LINE__, d->buffer_length >> 20, d->port, d->uri->ptr);
        } else {
            b = d->head;
            d->head = d->tail;
            b->used = 0;
            d->tail = b;
            d->tail->used = 0;
            if (verbose_mode)
                fprintf(stderr, "%s: (%s.%d) SWAP #2 SEGMENT WITH #1 SEGMENT FOR PORT #%d(%s)\n",
                        cur_ts_str, __FILE__, __LINE__, d->port, d->uri->ptr);
        }
        d->tail_pos = 0;
    }

    structlength = sizeof(server);
    r = recvfrom(fd, d->tail->ptr + d->tail->used, MAXLEN, 0, (struct sockaddr *) &server, &structlength);
    if (r <= 0) return;

    d->tail_pos = d->tail->used;
    d->tail->used += r;

    c = d->clients;
    while(c) {
        /* enable waiting connection to write */
        if (c->wait) stream_dvb_client(c);
        c = c->next;
    }
}

static void
dvb_http_conn_close(struct dvb_client *c)
{
    struct dvb_client *cn, *cp;

    if (c == NULL) return;
    if (c->fd > 0) {
        event_del(&(c->ev));
        close(c->fd);
        if (verbose_mode)
            fprintf(stderr, "%s: (%s.%d) CLOSE DVB CLIENT FD #%d (%s) -> PORT %d (%s)\n",
                cur_ts_str, __FILE__, __LINE__, c->fd, c->remote_ip, c->d?c->d->port:0, c->d?c->d->uri->ptr:"");
    }

    if (c->d) {
        cn = cp = c->d->clients;
        while (cn) {
            if (cn == c) break;
            cp = cn;
            cn = cn->next;
        }

        if (cn) {
            /* matched */
            if (cn == cp)
                c->d->clients = cn->next;
            else
                cp->next = cn->next;
        }
    }

    buffer_free(c->r);
    buffer_free(c->w);
    free(c);
}

static void
disable_dvb_client_write(struct dvb_client *c)
{
    if (c && c->fd > 0) {
        event_del(&c->ev);
        event_set(&c->ev, c->fd, EV_READ|EV_PERSIST, dvb_http_handler, (void *)c);
        event_add(&c->ev, 0);
        c->wait = 1;
    }
}

static void
enable_dvb_client_write(struct dvb_client *c)
{
    if (c && c->fd > 0) {
        event_del(&c->ev);
        event_set(&c->ev, c->fd, EV_READ|EV_WRITE|EV_PERSIST, dvb_http_handler, (void *)c);
        event_add(&c->ev, 0);
        c->wait = 0;
    }
}

static void
write_dvb_client(struct dvb_client *c)
{
    int r = 0, cnt = 0;
    struct iovec chunks[2];

    if (c == NULL || c->d == NULL) return;

    if (c->w->used > 0 && c->wpos < c->w->used) {
        chunks[0].iov_base = c->w->ptr + c->wpos;
        chunks[0].iov_len = c->w->used - c->wpos;
        cnt = 1;
    }

    if (c->live) {
        if (c->live->used > c->live_end) c->live_end = c->live->used;

        if (c->live_start < c->live_end) {
            chunks[cnt].iov_base = c->live->ptr + c->live_start;
            chunks[cnt].iov_len = c->live_end - c->live_start;
            cnt ++;
        }
    }

    if (cnt == 0) {
        disable_dvb_client_write(c);
        return;
    }

    r = writev(c->fd, (struct iovec *)&chunks, cnt);
    if (r < 0) {
            if (errno != EINTR && errno != EAGAIN) {
            dvb_http_conn_close(c);
            return;
        }
        r = 0;
    }

    if (c->w->used > 0 && c->wpos <= c->w->used) {
        if (r >= (c->w->used - c->wpos)) {
            r -= (c->w->used - c->wpos);
            c->wpos = c->w->used = 0;
        } else {
            c->wpos += r;
            r = 0;
        }
    }

    c->live_start += r;

    if (c->live->used > c->live_end)
        c->live_end = c->live->used;

    if (c->live_start == c->live_end) {
        disable_dvb_client_write(c);
    }
}

static void
parse_dvb_client_request(struct dvb_client *c)
{
    char *p, date[65], *q;
    struct dvb *d = NULL;

    if (c == NULL) return;

    strftime(date, sizeof("Fri, 01 Jan 1990 00:00:00 GMT")+1,
        "%a, %d %b %Y %H:%M:%S GMT", gmtime(&cur_ts));
    p = c->r->ptr + 4;
    if (strncmp(c->r->ptr, "GET /", 5) != 0) {
        c->http_status = 400; /* Bad Request */
        c->w->used = snprintf(c->w->ptr, c->w->size, "HTTP/1.0 400 Bad Request\r\n:Date: %s\r\n\r\n", date);
        return;
    }

    /* GET /1/250 HTTP/1.1\r\n
     * GET /channel/bitrate HTTP/1.1\r\n
     */

    /* "GET " */
    p = c->r->ptr + 4;

    q = strchr(p, ' ');
    if (q) *q = '\0';
    /* check tcp list first */
    d = dvb_tcp;
    while (d) {
        if (d->uri != NULL && 0 == strcmp(p, d->uri->ptr))
            break;
        d = d->next;
    }

    /* then check udp list */
    if (d == NULL) {
        d = dvb_udp;
        while (d) {
            if (d->uri != NULL && 0 == strcmp(p, d->uri->ptr) && d->tail != NULL)
                break;
            d = d->next;
        }
    }

    if (q) *q = ' ';
    if (d == NULL) {
        c->http_status = 404;
        c->w->used = snprintf(c->w->ptr, c->w->size, "HTTP/1.0 404 Not Found\r\n:Date: %s\r\n\r\n", date);
        return;
    }

    if (verbose_mode)
        fprintf(stderr, "%s: (%s.%d) NEW DVB CLIENT FD #%d (%s) -> PORT %d (%s)\n",
                cur_ts_str, __FILE__, __LINE__, c->fd, c->remote_ip, d->port, d->uri->ptr);

    c->d = d;
    c->next = d->clients;
    d->clients = c;
    c->wpos = 0;
    c->wait = 0;

    /* prepare http response header */
    c->w->used = snprintf(c->w->ptr, c->w->size,
            "HTTP/1.0 200 OK\r\nDate: %s\r\nServer: Youku DVB Receiver\r\n"
            "Expires: -1\r\nCache-Control: private, max-age=0\r\nPragma: no-cache\r\n"
            "\r\n",
            date);
    c->http_status = 0;
    c->live = NULL;
}

static void
dvb_http_handler(const int fd, const short which, void *arg)
{
    struct dvb_client *c;
    int r;
    char buf[129];

    c = (struct dvb_client *)arg;
    if (c == NULL) return;

    if (which & EV_READ) {
        switch(c->state) {
            case 0:
                r = read_buffer(fd, c->r);
                if (r < 0) {
                    dvb_http_conn_close(c);
                } else if (r > 0) {
                    /* terminate it with '\0' for following strstr */
                    c->r->ptr[c->r->used] = '\0';

                    if (strstr(c->r->ptr, "\r\n\r\n")) {
                        /* end of request http header */
                        parse_dvb_client_request(c);

                        if (c->http_status == 0) {
                            c->state = 1;
                            /* write HTTP header and first part of stream to client */
                            r = write(c->fd, c->w->ptr, c->w->used);
                            if (r < 0) {
                                if (errno != EINTR && errno != EAGAIN) {
                                    dvb_http_conn_close(c);
                                    return;
                                }
                                r = 0;
                            }

                            c->wpos = r;
                            enable_dvb_client_write(c);
                            if (c->wpos == c->w->used) {
                                /* finish of writing header */
                                c->wpos = c->w->used = 0;
                                stream_dvb_client(c);
                            }
                        } else {
                            write(c->fd, c->w->ptr, c->w->used);
                            dvb_http_conn_close(c);
                        }
                    }
                }
                break;
            case 1: /* write live flash stream */
                /* just drop incoming unused bytes */
                r = read(c->fd, buf, 128);
                if ((r == -1 && errno != EINTR && errno != EAGAIN) || r == 0)
                    dvb_http_conn_close(c);
                break;
        }
    } else if (which & EV_WRITE) {
        if (c->wpos < c->w->used || c->live) {
            write_dvb_client(c);
        } else {
            disable_dvb_client_write(c);
        }
    }

}

static void
dvb_tcp_conn_close(struct dvb *d)
{
    struct dvb_client *cn, *c;
    struct dvb *d1, *d2;

    if (d == NULL || d->is_udp == 1) return;
    if (d->fd > 0) {
        event_del(&(d->ev));
        close(d->fd);
        if (verbose_mode)
            fprintf(stderr, "%s: (%s.%d) CLOSE DVB TCP SERVER FD #%d -> PORT %d[%s]\n",
                cur_ts_str, __FILE__, __LINE__, d->fd, d->port, d->uri?d->uri->ptr:"");
    }

    /* close it's clients */
    cn = c = d->clients;
    while (c) {
        cn = c->next;
        if (c->fd > 0) {
            event_del(&(c->ev));
            close(c->fd);
            if (verbose_mode)
                fprintf(stderr, "%s: (%s.%d) CLOSE DVB CLIENT FD #%d -> PORT %d[%s]\n",
                    cur_ts_str, __FILE__, __LINE__, c->fd, d->port, d->uri?d->uri->ptr:"");
        }
        buffer_free(c->r);
        buffer_free(c->w);
        free(c);
        c = cn;
    }

    buffer_free(d->tail);
    buffer_free(d->head);
    buffer_free(d->uri);

    /* remove d from dvb_tcp list */
    if (dvb_tcp == d) {
        /* remove from head */
        dvb_tcp = d->next;
    } else if (dvb_tcp != NULL) {
        d2 = dvb_tcp;
        d1 = d2->next;
        while (d1) {
            if (d1 == d) {
                /* found, remove d from list */
                d2->next = d->next;
                break;
            }
            d2 = d1;
            d1 = d2->next;
        }
    }
    free(d);
}

#define TCP_BUFFER_LENGTH 64

static void
dvb_tcp_handler(const int fd, const short which, void *arg)
{
    struct dvb *d, *h;
    int r, changed = 0;
    char *p;
    struct dvb_client *c;
    struct buffer *b;

    d = (struct dvb *) arg;
    if (d == NULL || d->is_udp == 1 || 0 == (which & EV_READ)) return;

    if (d->tail == NULL) {
        d->tail = buffer_init(TCP_BUFFER_LENGTH * 1024 * 1024);
        if (d->tail == NULL) {
            fprintf(stderr, "%s: (%s.%d) OUT OF MEMORY FOR TCP SERVER FD #%d\n", cur_ts_str, __FILE__, __LINE__, fd);
            return;
        }
        if (verbose_mode)
            fprintf(stderr, "%s: (%s.%d) #1 %dMB SEGMENT FOR PORT #%d[%s]\n",
                    cur_ts_str, __FILE__, __LINE__, TCP_BUFFER_LENGTH, d->port, d->uri?d->uri->ptr:"");
    } else {
        if ((d->tail->used + MAXLEN) > d->tail->size) {
            /* we need to switch to new buffer */
            if (d->head == NULL) {
                b = buffer_init(TCP_BUFFER_LENGTH * 1024 * 1024);
                if (b == NULL) {
                    fprintf(stderr, "%s: (%s.%d) OUT OF MEMORY FOR TCP SERVER\n", cur_ts_str, __FILE__, __LINE__);
                    return;
                }
                d->head = d->tail;
                d->tail = b;
                if (verbose_mode)
                    fprintf(stderr, "%s: (%s.%d) #2 %dMB SEGMENT FOR PORT #%d[%s]\n",
                            cur_ts_str, __FILE__, __LINE__, TCP_BUFFER_LENGTH, d->port, d->uri?d->uri->ptr:"");
            } else {
                b = d->head;
                d->head = d->tail;
                b->used = 0;
                d->tail = b;
                d->tail->used = 0;
                if (verbose_mode)
                    fprintf(stderr, "%s: (%s.%d) TCP SWAP #2 SEGMENT WITH #1 SEGMENT FOR PORT #%d[%s]\n",
                            cur_ts_str, __FILE__, __LINE__, d->port, d->uri?d->uri->ptr:"");
            }
            d->tail_pos = 0;
        }
    }

    r = read_dvb_buffer(fd, d->tail);
    if (r <= 0) {
        if (r < 0) dvb_tcp_conn_close(d);
        return;
    } else {
        /* terminate it with '\0' for following strstr */
        d->tail->ptr[d->tail->used] = '\0';
    }

    if (d->tcp_state == 0) {
        /* header */
        /* PUT /xxxx/xxxx [DATA] */
        if (d->tail->used <= 4) return;

        if (strncmp(d->tail->ptr, "PUT ", 4) != 0) {
            /* wrong command */
            dvb_tcp_conn_close(d);
            return;
        }

        p = strchr(d->tail->ptr + 4, ' ');
        if (p == NULL) return; /* no URI */
        r = p - d->tail->ptr - 4;
        d->uri = buffer_init(r + 1);
        if (d->uri == NULL) {
            dvb_tcp_conn_close(d);
            return;
        }
        memcpy(d->uri->ptr, d->tail->ptr + 4, r);
        d->uri->used = r;
        d->uri->ptr[r] = '\0';

        memmove(d->tail->ptr, p + 1, d->tail->used - r - 5);
        d->tail->used -= r + 5;
        d->tail->ptr[d->tail->used] = '\0';
        /* look at dvb_tcp list */

        h = dvb_tcp;
        while (h) {
            if(strcmp(d->uri->ptr, h->uri->ptr) == 0) {
                /* duplicated */
                fprintf(stderr, "%s: (%s.%d) DUPLICATE TCP SERVER FOR URI \"%s\"\n", cur_ts_str, __FILE__, __LINE__, d->uri->ptr);
                dvb_tcp_conn_close(d);
                return;
            }
            h = h->next;
        }
        /* put d at head of dvb_tcp list */
        d->next = dvb_tcp;
        dvb_tcp = d;
        d->tail_pos = 0;
        if (d->tail->used > 0) changed = 1;
        d->tcp_state = 1;
        if (verbose_mode)
            fprintf(stderr, "%s: (%s.%d) NEW DVB TCP SERVER FD #%d -> TCP PORT #%d(%s)\n", cur_ts_str, __FILE__, __LINE__, d->fd, d->port, d->uri->ptr);
    } else {
        changed = 1;
    }

    if (changed) {
        c = d->clients;
        while(c) {
            /* enable waiting connection to write */
            if (c->wait) stream_dvb_client(c);
            c = c->next;
        }
    }
}

static void
dvb_tcp_accept(const int fd, const short which, void *arg)
{
    struct dvb *d = NULL;
    int newfd;
    struct sockaddr_in s_in;
    socklen_t len = sizeof(struct sockaddr_in);

    memset(&s_in, 0, len);
    newfd = accept(fd, (struct sockaddr *) &s_in, &len);
    if (newfd < 0) {
        fprintf(stderr, "%s: (%s.%d) fd %d accept() failed\n", cur_ts_str, __FILE__, __LINE__, fd);
        return ;
    }

    d = (struct dvb *) calloc(1, sizeof(*d));
    if (d == NULL) {
        fprintf(stderr, "%s: (%s.%d) out of memory for fd #%d\n", cur_ts_str, __FILE__, __LINE__, newfd);
        close(newfd);
        return;
    }

    d->uri = d->head = d->tail = NULL;
    d->clients = NULL;
    d->is_udp = 0;
    d->fd = newfd;
    d->tail_pos = 0;
    d->buffer_length = 0;
    d->port = tcp_port;

    set_nonblock(d->fd);

    d->tcp_state = 0;

    event_set(&(d->ev), d->fd, EV_READ|EV_PERSIST, dvb_tcp_handler, (void *) d);
    event_add(&(d->ev), 0);
}

static void
dvb_http_accept(const int fd, const short which, void *arg)
{
    struct dvb_client *c = NULL;
    int newfd;
    struct sockaddr_in s_in;
    socklen_t len = sizeof(struct sockaddr_in);
    char ip[20];

    memset(&s_in, 0, len);
    newfd = accept(fd, (struct sockaddr *) &s_in, &len);
    if (newfd < 0) {
        fprintf(stderr, "%s: (%s.%d) fd %d accept() failed\n", cur_ts_str, __FILE__, __LINE__, fd);
        return ;
    }

    strncpy(ip, inet_ntoa(s_in.sin_addr), 20);
    c = (struct dvb_client *) calloc(1, sizeof(*c));
    if (c == NULL) {
        fprintf(stderr, "%s: (%s.%d) out of memory for fd #%d\n", cur_ts_str, __FILE__, __LINE__, newfd);
        close(newfd);
        return;
    }

    if (c->r == NULL) c->r = buffer_init(2048);
    if (c->w == NULL) c->w = buffer_init(2048);

    if (c->r == NULL || c->w == NULL) {
        fprintf(stderr, "%s: (%s.%d) out of memory for fd #%d\n", cur_ts_str, __FILE__, __LINE__, newfd);
        close(newfd);
        return;
    }

    c->fd = newfd;
    set_nonblock(c->fd);

    c->state = c->http_status = 0;
    c->wait = c->wpos = 0;

    c->d = NULL;
    c->next = NULL;
    c->live = NULL;

    strncpy(c->remote_ip, ip, 20);
    event_set(&(c->ev), c->fd, EV_READ|EV_PERSIST, dvb_http_handler, (void *) c);
    event_add(&(c->ev), 0);
}

static void
receiver_timer_service(const int fd, short which, void *arg)
{
    struct timeval tv;

    cur_ts = time(NULL);
    strftime(cur_ts_str, 127, "%Y-%m-%d %H:%M:%S", localtime(&cur_ts));

    tv.tv_sec = 1; tv.tv_usec = 0; /* check for every 1 seconds */
    event_add(&ev_receiver_timer, &tv);
}

static void
dvb_receiver_service(void)
{
    struct timeval tv;
    struct dvb *d = NULL;

    event_init();

    event_set(&ev_http, http_fd, EV_READ|EV_PERSIST, dvb_http_accept, NULL);
    event_add(&ev_http, 0);

    if (tcp_fd > 0) {
        event_set(&ev_tcp, tcp_fd, EV_READ|EV_PERSIST, dvb_tcp_accept, NULL);
        event_add(&ev_tcp, 0);
    }

    /* timer */
    evtimer_set(&ev_receiver_timer, receiver_timer_service, NULL);
    tv.tv_sec = 1; tv.tv_usec = 0; /* check for every 1 seconds */
    event_add(&ev_receiver_timer, &tv);

    d = dvb_udp;
    while (d) {
        event_set(&d->ev, d->fd, EV_READ|EV_PERSIST, dvb_udp_handler, d);
        event_add(&d->ev, 0);
        d = d->next;
    }

    /* event loop */
    event_dispatch();
}

int
main(int argc, char **argv)
{
    int c, to_daemon = 1, i = 1;
    char *configfile = "/etc/flash/dvb.xml";
    struct sockaddr_in server, httpaddr, tcpaddr;
    struct dvb *d = NULL;

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
            show_dvb_help();
            return 1;
        }
    }

    if (configfile == NULL || configfile[0] == '\0') {
        fprintf(stderr, "Please provide -c file argument, or use default '/etc/flash/dvb.xml'\n");
        show_dvb_help();
        return 1;
    }

    cur_ts = time(NULL);
    strftime(cur_ts_str, 127, "%Y-%m-%d %H:%M:%S", localtime(&cur_ts));

    dvb_udp = dvb_tcp = NULL;

    if (parse_dvbconf(configfile)) {
        fprintf(stderr, "%s: (%s.%d) parse xml %s failed\n", cur_ts_str, __FILE__, __LINE__, configfile);
        return 1;
    }

    memset(&httpaddr, 0, sizeof(httpaddr));
    httpaddr.sin_family = AF_INET;
    httpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    httpaddr.sin_port = htons(http_port);

    if (0 == getuid()) {
        struct rlimit rlim;
        if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
            /* set max file number to 40k */
            rlim.rlim_cur = 40000;
            rlim.rlim_max = 40000;
            if (0 != setrlimit(RLIMIT_NOFILE, &rlim)) {
                fprintf(stderr, "(%s.%d) can't set 'max filedescriptors': %s\n", __FILE__, __LINE__, strerror(errno));
                return 1;
            }
        }
    }

    /* http */
    http_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (http_fd < 0) {
        fprintf(stderr, "%s: (%s.%d) can't create http socket\n", cur_ts_str, __FILE__, __LINE__);
        return 1;
    }

    set_nonblock(http_fd);
    if (bind(http_fd, (struct sockaddr *) &httpaddr, sizeof(struct sockaddr)) || listen(http_fd, 512)) {
        fprintf(stderr, "%s: (%s.%d) fd #%d can't bind/listen: %s\n", cur_ts_str, __FILE__, __LINE__, http_fd, strerror(errno));
        close(http_fd);
        return 1;
    }

    /* tcp */
    if (tcp_port > 0) {
        memset(&tcpaddr, 0, sizeof(tcpaddr));
        tcpaddr.sin_family = AF_INET;
        tcpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        tcpaddr.sin_port = htons(tcp_port);

        tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_fd < 0) {
            fprintf(stderr, "%s: (%s.%d) can't create tcp socket\n", cur_ts_str, __FILE__, __LINE__);
            return 1;
        }

        set_nonblock(tcp_fd);
        if (bind(tcp_fd, (struct sockaddr *) &tcpaddr, sizeof(struct sockaddr)) || listen(tcp_fd, 512)) {
            fprintf(stderr, "%s: (%s.%d) fd #%d can't bind/listen: %s\n", cur_ts_str, __FILE__, __LINE__, tcp_fd, strerror(errno));
            close(tcp_fd);
            return 1;
        }
    }

    fprintf(stderr, "%s: Youku Live DVB Receiver v%s(" __DATE__ " " __TIME__ ")\n",
            cur_ts_str, VERSION);

    fprintf(stderr, "\tRECEIVER HTTP -> TCP #%d\n", http_port);

    if (tcp_port > 0) fprintf(stderr, "\t#0 DVB TCP SERVER PORT -> #%d\n", tcp_port);

    d = dvb_udp;
    while (d) {
        if (d->port > 0 && d->uri) {
            d->fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (d->fd > 0) {
                set_nonblock(d->fd);
                memset((char *) &server, 0, sizeof(server));
                server.sin_family = AF_INET;
                server.sin_addr.s_addr = htonl(INADDR_ANY);
                server.sin_port = htons(d->port);
                if (bind(d->fd, (struct sockaddr *) &server, sizeof(server))) {
                    if (errno != EINTR)
                        fprintf(stderr, "%s: (%s.%d) errno = %d: %s\n", cur_ts_str, __FILE__, __LINE__, errno, strerror(errno));
                    close(d->fd);
                    return 1;
                }

                fprintf(stderr, "\t#%d UDP PORT %d -> URL \"%s\"\n", i, d->port, d->uri->ptr);
            } else {
                fprintf(stderr, "%s: (%s.%d) can't create udp socket\n", cur_ts_str, __FILE__, __LINE__);
                return 1;
            }
        }
        d = d->next;
        ++ i;
    }

    if (to_daemon && daemon(0, 0) == -1) {
        fprintf(stderr, "%s: (%s.%d) failed to be a daemon\n", cur_ts_str, __FILE__, __LINE__);
        exit(1);
    }

    /* ignore SIGPIPE when write to closing fd
     * otherwise EPIPE error will cause transporter to terminate unexpectedly
     */
    signal(SIGPIPE, SIG_IGN);

    dvb_receiver_service();
    return 0;
}
