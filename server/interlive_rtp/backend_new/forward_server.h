/**
* @file forward_server.h
* @brief 
*
* @author
* @copyright Youku All rights reserved.
* @email
* @company http://www.youku.com
* @date 2016-2-16
*/
#ifndef _FORWARD_SERVER_H
#define _FORWARD_SERVER_H

#include <map>

#include "proto_define.h"
#include "streamid.h"
#include "cache_manager.h"
#include "module_backend.h"
#include "utils/buffer.hpp"
#include "util/hashtable.h"

#include "tcp_connection.h"
#include "cmd_fsm_state.h"
#include "cmd_base_role_mgr.h"
#include "cmd_fsm.h"
#include "../network/base_http_server.h"

typedef struct fs_stream
{
    uint32_t stream_id;
    int alive;
    session timeout_ses;
    uint16_t session_count;
    hashtable* sessions;
    hashnode hash_entry;
} fs_stream;


int fs_start_stream(uint32_t streamid);
int fs_stop_stream(uint32_t streamid);

namespace interlive
{
namespace forward_server
{

class ForwardConnection;
struct FSStream
{
	StreamId_Ext    streamid;
	time_t          last_active_time;
	bool            alive;

	FSStream() : streamid(0), last_active_time(0), alive(false) {}
};
/**
 * @brief
 */
class ForwardServer
{
public:
    typedef std::map<StreamId_Ext, FSStream> fs_streams_t;

public:
    static ForwardServer* get_server();
    static void release();

    bool create(const backend_config& backend_conf);

    void on_second(time_t t);
    /**
     * @brief polling for stream
     * @return
     */
    void on_timeout(time_t t);
    ForwardConnection* get_forward_conn(uint32_t id);

public:
    void add_stream_to_tracker(StreamId_Ext stream_id, int level);
    void del_stream_from_tracker(StreamId_Ext stream_id, int level);
    void update_all_streams_to_tracker();

    uint32_t get_stream_count() const;
    bool has_register_tracker() const;
	//void set_http_server(http::HTTPServer* http_server);
 private:
    ForwardServer();
    virtual ~ForwardServer();
    ForwardServer(const ForwardServer&);
    ForwardServer& operator=(const ForwardServer&);


    void _init_tracker_conn();
    bool _report_add_stream(StreamId_Ext stream_id);
    bool _report_to_tracker(StreamId_Ext stream_id, int action, int level);

    void _connect_tracker();
    void _register_to_tracker();
    void _tracker_keepalive();
    static void _data_callback_by_cm(StreamId_Ext stream_id, uint8_t watch_type, void* arg);

    lcdn::net::CMDFSMState* _get_init_state();

    void _destroy();
    void _write();



private:

    const backend_config* _backend_conf;

    fs_streams_t _fs_streams;
    time_t _dump_time_last;
    uint32_t _max_session_num;
private:
    static ForwardServer *_g_server;

};
} // namespace forward_server
} // namespace interlive
#endif

