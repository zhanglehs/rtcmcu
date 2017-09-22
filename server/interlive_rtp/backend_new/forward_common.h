/*
* forward_common.h
*
*  Created on: 2013-7-15
*      Author: zzhang
*/

#ifndef FORWARD_COMMON_H_
#define FORWARD_COMMON_H_
#include "util/hashtable.h"
#include "utils/memory.h"
#include <arpa/inet.h>
#include <common/proto.h>
#include <util/levent.h>
#include <util/session.h>

#include <iostream>
#include <iomanip>
#include <string>
using std::string;

#include "common_defs.h"

#if (defined __cplusplus && !defined _WIN32)
extern "C"
{
#endif
    /*
    * common connection facility
    */

    typedef enum
    {
        CONN_STATE_INIT, CONN_STATE_CONNECTING, CONN_STATE_CONNECTED, CONN_STATE_NOT_INIT,
    } conn_state;

    struct connection_base;

    typedef void
        (*conn_on_connect)(struct connection_base* conn);

    typedef void(*conn_on_read)(struct connection_base* conn, uint16_t cmd);

    typedef void(*conn_on_write)(struct connection_base* conn);

    typedef void(*conn_on_error)(struct connection_base* conn, short which);

    typedef void(*conn_on_close)(struct connection_base* conn);

    typedef struct connection_base
    {
        uint32_t ip;
        uint16_t port;
        int fd;
        conn_state state;
        struct event ev;
        buffer *rb, *wb;
        conn_on_connect on_connect;
        conn_on_read on_read;
        conn_on_write on_write;
        conn_on_error on_error;
        conn_on_close on_close;
        uint64_t read_bytes;
        uint64_t write_bytes;

        //v3 supported
        void* owner;

        uint32_t id() { return static_cast<uint32_t>(fd); }
    } connection_base;

    void set_event_base(struct event_base *mainbase);

    struct event_base * get_event_base();

    uint64_t compute_connection_id(uint32_t ip, uint16_t port, uint32_t stream_id = 0);

    //int init_connection(connection_base* conn);

    int init_connection_by_addr(connection_base* conn, uint32_t ip, uint16_t port);

    int init_connection_by_addr_v2(connection_base* conn, uint32_t ip, uint16_t port, uint32_t read_max_size, uint32_t write_max_size);

    void close_connection(connection_base* conn);

    void reset_connection(connection_base* c);

    typedef int(*encode_func)(const void* msg, buffer* buf);

    int encode_msg(const void* msg, encode_func func, connection_base* conn);
    int encode_fs_msg(const void* msg, encode_func func, connection_base* conn);
    int encode_msg_new(const void* msg, encode_func func, connection_base* conn);

    typedef int(*decode_func)(buffer* buf);

    const void* decode_msg(decode_func func, connection_base* conn);
    const void* decode_msg_new(decode_func func, connection_base* conn);

    const void* decode_msg_v2(decode_func func, buffer *rb);

    void register_conn_handler(connection_base* conn);

    void disable_write(connection_base* c);

    void enable_write(connection_base* c);

    int do_connect(connection_base* conn);

    /*
    * forword level management functions
    */

    int init_forward_levels();

    void free_forward_levels();

    int get_forward_level(uint32_t streamid, uint16_t *level);

    int set_forward_level(uint32_t streamid, uint16_t level);

    void delete_forward_level(uint32_t streamid);

    int get_forward_level_v2(StreamId_Ext streamid, uint16_t *level);

    int set_forward_level_v2(StreamId_Ext streamid, uint16_t level);

    void delete_forward_level_v2(StreamId_Ext streamid);

    /*
    * report
    */
    typedef enum
    {
        FORWARD_STAT = 0,
        EVENT_STREAM_START = 1,
        EVENT_STREAM_ADDR_GOT = 2,
        EVENT_STREAM_HEADER = 3,
        EVENT_STREAM_DATA = 4,
        EVENT_STREAM_TIMEOUT = 5,
        EVENT_STREAM_STOP = 6
    } forward_report_type;

    typedef struct forward_stat
    {
        uint16_t stream_count;
        uint16_t stream_alive_count;
        uint16_t fc_conn_count;
        uint16_t fs_conn_count;
        //    uint32_t fc_bps;
        //    uint32_t fs_bps;
    } forward_stat;

    extern forward_stat g_forward_stat;

    struct NetEndpoint
    {
        // UDP socket endpoint of client
        in_addr_t ip;
        short port;

        NetEndpoint() : ip(0), port(0) {}
        NetEndpoint(in_addr_t _ip, short _port) : ip(_ip), port(_port) {}
        NetEndpoint(const NetEndpoint& rhv) : ip(rhv.ip), port(rhv.port) {}

        bool operator==(const NetEndpoint& rhv) const
        {
            return (ip == rhv.ip) && (port == rhv.port);
        }

        bool operator<(const NetEndpoint& rhv) const
        {
            return ip < rhv.ip || (ip == rhv.ip && port < rhv.port);
        }

        void operator=(const NetEndpoint& rhv)
        {
            ip = rhv.ip;
            port = rhv.port;
        }
    };

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif /* FORWARD_COMMON_H_ */
