#ifndef __PORTAL_H_
#define __PORTAL_H_

#include "util/levent.h"
#include "utils/buffer.hpp"
#include "util/log.h"
#include <map>
#include "streamid.h"
#include "common/proto_define.h"
#include "config_manager.h"

#include "module_tracker.h"

#define PORTAL_BUF_MAX (20 * 1024 * 1024)

class Portal
{
public:
	Portal(bool enable_uploader, bool enable_player);
	
	int init();

	static Portal* get_instance();

	void reset();

	void inf_stream(uint32_t * ids, uint32_t cnt);

    void inf_stream(stream_info* info, uint32_t cnt);

    int parse_cmd(buffer *rb);

	void keepalive();

    void register_create_stream(void(*handler)(stream_info&));

//    void register_create_stream(void(*handler)(uint32_t, timeval* start_time));

    void register_destroy_stream(void (*handler)(uint32_t));

    void register_f2p_keepalive_handler(f2p_keepalive* (*handler)(f2p_keepalive*));

    void register_r2p_keepalive_handler(r2p_keepalive* (*handler)(r2p_keepalive*));

    ~Portal();

    static void destroy();

public:
	static Portal* instance;

private:

	char *_rkeepalive;
	char *_fkeepalive;

	uint16_t _stream_cnt;
	uint32_t *_streamids;

    typedef std::map<uint32_t, stream_info> StreamMap_t;
    StreamMap_t _stream_map;

	bool _enable_uploader;
	bool _enable_player;

    void(*_create_stream_handler)(stream_info&);
    void(*_destroy_stream_handler)(uint32_t);

//    void(*_create_stream_handler_v2)(stream_info);

    f2p_keepalive* (*_f2p_keepalive_handler)(f2p_keepalive*);
    r2p_keepalive* (*_r2p_keepalive_handler)(r2p_keepalive*);

    int32_t _info_stream_beats_cnt;
    int32_t _player_keepalive_cnt;
    int32_t _uploader_keepalive_cnt;
};


class ModTrackerWhitelistClient : public ModTrackerClientBase
{
public:
    ModTrackerWhitelistClient(Portal *portal);

    virtual ~ModTrackerWhitelistClient();

    /*
    call example:
    proto_header ph = {0, 0, CMD_FS2T_REGISTER_REQ_V2, 0};
    f2t_register_req_v2 = {something};
    proto_wrapper pw = {&ph, &f2t_register_req_v2};
    mod_tracker_inst.send_request(&pw, &mod_tracker_fs_client_inst);
    */
    virtual int encode(const void* data, buffer* wb);

    virtual int decode(buffer* rb);

    virtual void on_error(short which)
    {
        WRN("ModTrackerWhitelistClient::on_error, which=%d", int(which));
    }

    void reset_status()
    {
    }
    
    private:
        Portal *_portal;
};
#endif /* __PORTAL_H_ */
