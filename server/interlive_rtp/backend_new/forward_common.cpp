/*
* forward_common.c
*
*  Created on: 2013-7-15
*      Author: zzhang
*/
#include "forward_common.h"
#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <event.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>
#include "util/log.h"
#include "util/util.h"
#include <string.h>

static struct event_base * g_ev_base = NULL;

void set_event_base(struct event_base *ev_base)
{
    g_ev_base = ev_base;
}

struct event_base * get_event_base()
{
    return g_ev_base;
}

static void handle_read(connection_base* c)
{
    int ret = 1;
    while (ret > 0)
    {
        ret = buffer_read_fd(c->rb, c->fd);
        TRC("handle_read, fd=%d, ip=%s, port=%d, read_len=%d",
            c->fd, util_ip2str(c->ip), c->port, ret);
    }

    if (ret < 0) {
        WRN("handle_read: read fd error errno=%d, err=%s, ip=%s, port=%hu, fd=%d", errno, strerror(errno),
            util_ip2str(c->ip), c->port, c->fd);
        c->on_error(c, EV_READ);
        return;
    }
    c->read_bytes += ret;
    if (buffer_data_len(c->rb) < sizeof(proto_header)) {
        return;
    }
    while (buffer_data_len(c->rb) >= sizeof(proto_header))
    {
        proto_header pro_head;
        if (decode_header(c->rb, &pro_head) < 0) {
            ERR("handle_read: DECODE HEADER ERROR");
            c->on_error(c, EV_READ);
            return;
        }
        if (buffer_data_len(c->rb) < pro_head.size) {
            break;
        }
        c->on_read(c, pro_head.cmd);
        if (c->fd == -1) {
            break;
        }

        if (pro_head.size < sizeof(proto_header))
        {
            ERR("handle_read: pro_head.size is invalid");
            c->on_error(c, EV_READ);
            return;
        }

        TRC("handle_read: eat %d bytes", pro_head.size);
        buffer_eat(c->rb, pro_head.size);
    }

    if (c->fd != -1)
    {
        buffer_try_adjust(c->rb);
        TRC("handle_read: end handle_read, fd=%d, ip=%s, port=%d, buffer_len=%lu",
            c->fd, util_ip2str(c->ip), c->port, buffer_data_len(c->rb));

        if (buffer_data_len(c->wb) > 0)
        {
            TRC("handle_read: enable_write, fd:%d, ip:%s, port:%d", c->fd, util_ip2str(c->ip), c->port);
            enable_write(c);
        }
    }
}

static void handle_write(connection_base* c)
{
    TRC("handle_write: fd=%d, ip=%s, port=%d", c->fd, util_ip2str(c->ip), c->port);
    if (c->state == CONN_STATE_CONNECTING) {
        c->state = CONN_STATE_CONNECTED;
        if (c->on_connect) {
            c->on_connect(c);

        }
    }
    if (c->on_write) {
        c->on_write(c);
    }
    if (buffer_data_len(c->wb) > 0) {
        int ret = buffer_write_fd(c->wb, c->fd);
        TRC("handle_write: fd=%d, ip=%s, port=%d, len=%d",
            c->fd, util_ip2str(c->ip), c->port, ret);
        if (ret < 0) {
            WRN("handle_write: write fd error, errno=%d, err=%s, ip=%s, port=%hu", errno,
                strerror(errno), util_ip2str(c->ip), c->port);
            c->on_error(c, EV_WRITE);
            return;
        }
        c->write_bytes += ret;
        buffer_try_adjust(c->wb);
    }

    if (buffer_data_len(c->wb) == 0) {
        TRC("handle_write: disable_write, fd=%d, ip=%s, port=%d", c->fd, util_ip2str(c->ip), c->port);
        disable_write(c);
    }
}

static void conn_handler(int fd, short which, void* arg)
{
    connection_base* conn = (connection_base*)arg;
    assert(conn != NULL);

    if (which & EV_READ) {
        handle_read(conn);
    }
    else if (which & EV_WRITE) {
        handle_write(conn);
    }
}

void enable_write(connection_base* c)
{
    TRC("enable_write: enable write");
    levent_del(&(c->ev));
    levent_set(&(c->ev), c->fd, EV_READ | EV_WRITE | EV_PERSIST, conn_handler,
        c);
    event_base_set(g_ev_base, &(c->ev));
    levent_add(&(c->ev), 0);
}

void disable_write(connection_base* c)
{
    TRC("disable_write: disable write");
    levent_del(&(c->ev));
    levent_set(&(c->ev), c->fd, EV_READ | EV_PERSIST, conn_handler, c);
    event_base_set(g_ev_base, &(c->ev));
    levent_add(&(c->ev), 0);
}

void register_conn_handler(connection_base* conn)
{
    levent_set(&conn->ev, conn->fd, EV_READ | EV_WRITE | EV_PERSIST,
        conn_handler, (void *)conn);
    event_base_set(g_ev_base, &(conn->ev));
    levent_add(&conn->ev, 0);
}

int do_connect(connection_base* conn)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(conn->ip);
    addr.sin_port = htons(conn->port);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        ERR("do_connect: %s, errno=%d, err=%s", "create socket error", errno, strerror(errno));
        return -1;
    }
    util_set_nonblock(sock, TRUE);
    conn->fd = sock;

    int buf_size = 0;
    socklen_t opt_size = 0;

    getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buf_size, &opt_size);
    DBG("do_connect: socket read buffer size=%d, ip=%s, port=%d",
        buf_size, util_ip2str(conn->ip), conn->port);

    int sock_rcv_buf_sz = 512 * 1024;
    if (-1 == setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
        (void*)&sock_rcv_buf_sz, sizeof(sock_rcv_buf_sz)))
    {
        ERR("do_connect: set forward sock rcv buf sz failed. error=%d %s", errno, strerror(errno));
        close(sock);
        sock = -1;
        return -1;
    }

    getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buf_size, &opt_size);
    DBG("do_connect: socket read buffer size=%d, ip=%s, port=%d",
        buf_size, util_ip2str(conn->ip), conn->port);

    int r = connect(sock, (struct sockaddr*) &addr, sizeof(addr));

    DBG("do_connect: connect rtn: %d, errno: %d", r, errno);

    if (r < 0) {
        if ((errno != EINPROGRESS) && (errno != EINTR)) {
            WRN("connect error: %d %s", errno, strerror(errno));
            return -1;
        }
        conn->state = CONN_STATE_CONNECTING;
    }
    else if (r == 0) {
        conn->state = CONN_STATE_CONNECTED;
        if (conn->on_connect) {
            conn->on_connect(conn);
        }
    }
    register_conn_handler(conn);
    return 0;
}

// suggests use encode_msg_new instead
int encode_msg(const void* msg, encode_func func, connection_base* conn)
{
    size_t used = buffer_data_len(conn->wb);
    int ret = func(msg, conn->wb);
    if (ret < 0 && conn->on_error) {
        // TODO 
        // should not call conn->on_error cause the error is not caused by read/write on conn
        // suggests use encode_msg_new instead 
        WRN("encode_msg: error, fd=%d, ip=%s, port=%d",
            conn->fd, util_ip2str(conn->ip), conn->port);
        conn->on_error(conn, EV_WRITE);
    }
    if (used == 0 && ret == 0) {
        TRC("encode_msg: enable_write, fd=%d, ip=%s, port=%d", conn->fd, util_ip2str(conn->ip), conn->port);
        enable_write(conn);
    }
    return ret;
}

int encode_fs_msg(const void* msg, encode_func func, connection_base* conn)
{
    size_t used = buffer_data_len(conn->wb);
    int ret = func(msg, conn->wb);
    if (ret == 0)
    {
        // should not call conn->on_error cause the error is not caused by read/write on conn
        WRN("encode_fs_msg: failed, fd=%d, ip=%s, port=%d",
            conn->fd, util_ip2str(conn->ip), conn->port);
        //conn->on_error(conn, EV_WRITE);
    }
    if (used == 0 && ret > 0) {
        TRC("encode_fs_msg: enable_write, fd=%d, ip=%s, port=%d", conn->fd, util_ip2str(conn->ip), conn->port);
        enable_write(conn);
    }
    return ret;
}

int encode_msg_new(const void* msg, encode_func func, connection_base* conn)
{
    size_t used = buffer_data_len(conn->wb);
    int ret = func(msg, conn->wb);
    /*
    if (ret < 0 && conn->on_error) {
    conn->on_error(conn, EV_WRITE);
    }
    */
    //    if (used == 0 && ret == 0) {
    if (used == 0 || ret == 0) {
        enable_write(conn);
    }
    return ret;
}

const void* decode_msg(decode_func func, connection_base* conn)
{
    int ret = func(conn->rb);
    if (ret < 0) {
        if (conn->on_error) {
            ERR("decode_msg: error, fd=%d, ip=%s, port=%d",
                conn->fd, util_ip2str(conn->ip), conn->port);
            conn->on_error(conn, EV_READ);
        }
        return NULL;
    }
    return buffer_data_ptr(conn->rb) + sizeof(proto_header);
}

// suggests use decode_msg_new instead
const void* decode_msg_new(decode_func func, connection_base* conn)
{
    int ret = func(conn->rb);
    if (ret < 0) {
        /*
        if (conn->on_error) {
        conn->on_error(conn, EV_READ);
        }
        */
        return NULL;
    }
    return buffer_data_ptr(conn->rb) + sizeof(proto_header);
}

const void* decode_msg_v2(decode_func func, buffer *rb)
{
    int ret = func(rb);
    if (ret < 0) {
        return NULL;
    }
    return buffer_data_ptr(rb) + sizeof(proto_header);
}

uint64_t compute_connection_id(uint32_t ip, uint16_t port, uint32_t stream_id)
{
    uint64_t id = ip;
    return (id << 32) + ((stream_id << 16) & 0xffff0000) + port;
}

int init_connection_by_addr(connection_base* conn, uint32_t ip, uint16_t port)
{
    memset(conn, 0, sizeof(connection_base));
    conn->ip = ip;
    conn->port = port;
    conn->rb = buffer_create_max(BUFFER_INIT_SIZE, BUFFER_MAX_SIZE);
    conn->wb = buffer_create_max(BUFFER_INIT_SIZE, BUFFER_MAX_SIZE);
    if (conn->rb == NULL || conn->wb == NULL) {
        if (conn->rb)
            buffer_free(conn->rb);
        if (conn->wb)
            buffer_free(conn->wb);
        return -1;
    }
    conn->fd = -1;

    //conn->state = -1;
    // hechao changed initial state to CONN_STATE_INIT instead of -1
    conn->state = CONN_STATE_INIT;
    conn->on_connect = NULL;
    conn->on_read = NULL;
    conn->on_write = NULL;
    conn->on_error = NULL;
    return 0;
}

int init_connection_by_addr_v2(connection_base* conn, uint32_t ip, uint16_t port, uint32_t read_max_size, uint32_t write_max_size)
{
    memset(conn, 0, sizeof(connection_base));
    conn->ip = ip;
    conn->port = port;
    conn->rb = buffer_create_max(read_max_size, read_max_size);
    conn->wb = buffer_create_max(write_max_size, write_max_size);
    if (conn->rb == NULL || conn->wb == NULL) {
        if (conn->rb)
            buffer_free(conn->rb);
        if (conn->wb)
            buffer_free(conn->wb);
        return -1;
    }
    conn->fd = -1;

    //conn->state = -1;
    // hechao changed initial state to CONN_STATE_INIT instead of -1
    conn->state = CONN_STATE_INIT;
    conn->on_connect = NULL;
    conn->on_read = NULL;
    conn->on_write = NULL;
    conn->on_error = NULL;
    return 0;
}


void close_connection(connection_base* c)
{
    if (NULL == c){
        return;
    }
    DBG("close_connection: fd=%d", c->fd);

    c->state = CONN_STATE_INIT;

    if (c->fd > 0) {
        close(c->fd);
        c->fd = -1;
        levent_del(&(c->ev));
    }
    if (c->rb) {
        buffer_free(c->rb);
        c->rb = NULL;
    }
    if (c->wb) {
        buffer_free(c->wb);
        c->wb = NULL;
    }
}

void reset_connection(connection_base* c)
{
    DBG("reset_connection: fd=%d", c->fd);
    if (c->fd > 0) {
        close(c->fd);
        c->fd = -1;
        levent_del(&(c->ev));
    }
    buffer_reset(c->rb);
    buffer_reset(c->wb);
    c->state = CONN_STATE_INIT;
}

typedef struct forward_level
{
    uint16_t level;
    hashnode hash_entry;
} forward_level;

static hashtable* forward_levels = NULL;

int init_forward_levels()
{
    forward_levels = hash_create(10);
    if (forward_levels == NULL)
        return -1;
    return 0;
}

static void free_forward_level(hashnode * node)
{
    forward_level *fl = container_of(node, forward_level, hash_entry);
    mfree(fl);
}

void free_forward_levels()
{
    hash_destroy(forward_levels, free_forward_level);
    forward_levels = NULL;
}

int get_forward_level(uint32_t streamid, uint16_t *level)
{
    hashnode *node = hash_get(forward_levels, streamid);
    if (NULL == node) {
        return -1;
    }
    forward_level *fl = container_of(node, forward_level, hash_entry);
    *level = fl->level;
    return 0;
}

int get_forward_level_v2(StreamId_Ext streamid, uint16_t *level)
{
    hashnode *node = hash_get(forward_levels, streamid.get_32bit_stream_id());
    if (NULL == node) {
        return -1;
    }
    forward_level *fl = container_of(node, forward_level, hash_entry);
    *level = fl->level;
    return 0;
}

int set_forward_level(uint32_t streamid, uint16_t level)
{
    hashnode *node = hash_get(forward_levels, streamid);
    if (NULL == node) {
        forward_level *fl = (forward_level*)mmalloc(sizeof(forward_level));
        if (!fl) {
            return -1;
        }
        fl->level = level;
        hash_insert(forward_levels, streamid, &fl->hash_entry);
    }
    else {
        forward_level *fl = container_of(node, forward_level, hash_entry);
        fl->level = level;
    }
    return 0;
}

int set_forward_level_v2(StreamId_Ext streamid, uint16_t level)
{
    hashnode *node = hash_get(forward_levels, streamid.get_32bit_stream_id());
    if (NULL == node) {
        forward_level *fl = (forward_level*)mmalloc(sizeof(forward_level));
        if (!fl) {
            return -1;
        }
        fl->level = level;
        hash_insert(forward_levels, streamid.get_32bit_stream_id(), &fl->hash_entry);
    }
    else {
        forward_level *fl = container_of(node, forward_level, hash_entry);
        fl->level = level;
    }
    return 0;
}

void delete_forward_level(uint32_t streamid)
{
    hashnode *node = hash_delete(forward_levels, streamid);
    if (NULL != node) {
        free_forward_level(node);
    }
}

void delete_forward_level_v2(StreamId_Ext streamid)
{
    hashnode *node = hash_delete(forward_levels, streamid.get_32bit_stream_id());
    if (NULL != node) {
        free_forward_level(node);
    }
}

forward_stat g_forward_stat;

void init_forward_stat()
{
    g_forward_stat.stream_count = 0;
    g_forward_stat.stream_alive_count = 0;
    g_forward_stat.fc_conn_count = 0;
    g_forward_stat.fs_conn_count = 0;
    //    g_forward_stat.fc_bps = 0;
    //    g_forward_stat.fs_bps = 0;
}
