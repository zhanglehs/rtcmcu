#ifndef __PORTAL_H_
#define __PORTAL_H_

#include "util/levent.h"
#include "utils/buffer.hpp"
#include "util/log.h"
#include <map>
#include "../interlive/streamid.h"
#include "common/proto_define.h"
#include "tracker_config.h"
#include "stream_manager.h"
#include "forward_manager_v3.h"

#define PORTAL_BUF_MAX (20 * 1024 * 1024)

typedef enum
{
    PORTAL_CLOSED = 0, 
    PORTAL_CONNECTING = 1, 
    PORTAL_CONNECTED = 2,
} portal_state;

class Portal
{
public:
    Portal(event_base* main_base, bool enable_uploader, StreamManager* stream_manager, ForwardServerManager *forward_manger);

    int init();

    static Portal* get_instance();

    void reset();

    void inf_stream(stream_info* info, uint32_t cnt);

    void parse_cmd();

    void keepalive();

    void socket_handler(const int fd, const short which, void *arg);

    void check_connection(tracker_config* config);

    void register_create_stream(void(*handler)(StreamItem&));

    void register_destroy_stream(void (*handler)(uint32_t));

    void register_f2p_keepalive_handler(f2p_keepalive* (*handler)(f2p_keepalive*));

    ~Portal();

    static void destroy();
private:

    void _enable_write();
    void _disable_write();

public:
    static Portal* instance;

private:
    buffer *_read_buf;
    buffer *_write_buf;

    int _fd;

    portal_state _state;

    struct event _ev;

    char *_fkeepalive;

    event_base *_ev_base;

    bool _enable_uploader;

    void(*_create_stream_handler)(StreamItem&);
    void(*_destroy_stream_handler)(uint32_t);

    f2p_keepalive* (*_f2p_keepalive_handler)(f2p_keepalive*);

    uint32_t _info_stream_beats_cnt;

    StreamManager* _stream_manager;
    ForwardServerManager *_forward_manager;

    uint16_t _seq;
};

#endif /* __PORTAL_H_ */
