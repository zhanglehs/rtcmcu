/***********************************************************************
 * author: zhangcan
 * email: zhangcan@youku.com
 * date: 2015-02-13
 * copyright:youku.com
 ************************************************************************/

#ifndef _BASE_CLIENT_H_
#define _BASE_CLIENT_H_

#include <stdint.h>
#include <map>
//#include <ext/hash_map>
#include <vector>
#include <stddef.h>

#include "streamid.h"
#include "module_player.h" 
#include "cache_manager.h"
#include "connection.h"

struct connection;
class BaseClient;

class player_stat_t {
public:
    player_stat_t() : online_cnt(0) {}
    void add_online_cnt() { online_cnt++; }
    void minus_online_cnt() {
        if (0 == online_cnt) {
            wrn_cnt++;
            wrn_cnt_total++;
        }
        else {
            online_cnt--;
        }
    }
    void player_err() {
        err_cnt++;
        err_cnt_total++;
    }
    uint32_t get_online_cnt() const { return online_cnt; }
private:
    uint32_t      err_cnt;
    uint32_t      wrn_cnt;
    uint32_t      err_cnt_total;
    uint32_t      wrn_cnt_total;
    uint32_t      m3u8_srv_cnt;
    uint32_t      ts_srv_cnt;
    uint32_t      live_online_cnt;
    uint32_t      online_cnt;
};

class PlayRequest
{
public:
    enum PLAY_REQUEST_TYPE
    {
        RequestFragment = 0,
        RequestBlock = 1,
        RequestLive = 2,
        RequestLiveAudio = 3,
        RequestHLSTSSegment = 4,
        RequestHLSM3U8 = 5,
        RequestP2PMetadata = 6,
        RequestP2PTimerange = 7,
        RequestP2PSegment = 8,
        RequestP2PRandomSegment = 9,
        RequestP2PLive = 10,
        RequestSDP = 11, 
        RequestFlvMiniBlock = 12,
        RequestInvalid
    };

    PlayRequest(StreamId_Ext sid = StreamId_Ext(),
        uint32_t fseq = 0,
        uint32_t gbseq = 0,
        media_manager::RateType rt = media_manager::RATE_COMPLETE,
        PlayRequest::PLAY_REQUEST_TYPE requesttype = PlayRequest::RequestLive)
        :header_flag(FLV_FLAG_BOTH),
        _stream_id(sid),
        _fragment_seq(fseq),
        _global_block_seq(gbseq),
        _p2p_block_num(0),
        _rt(rt),
        _request_type(requesttype),
        _quick_start(false),
        _pre_seek_ms(0),
        _speed_up(0),
        _key_frame_start(true)
    {}

    const char* type_to_string()
    {
        switch (_request_type)
        {
        case PlayRequest::RequestLive:
        case PlayRequest::RequestLiveAudio:
        case PlayRequest::RequestFragment:
        case PlayRequest::RequestFlvMiniBlock:
            return "flv";
        case PlayRequest::RequestHLSTSSegment:
            return "ts";
        default:
            return "unknown";
        }
    }

    void set_sid(const StreamId_Ext& sid)
    {
        _stream_id = sid;
    }

    void set_method(const char * method)
    {
        _method = method;
    }
    
    void set_path(const char * path)
    {
        _path = path;
    }

    void set_query(const char * query)
    {
        _query = query;
    }

    void set_token(const std::string& token)
    {
        _token = token;
    }

    void set_fragment_seq(uint32_t fseq)
    {
        _fragment_seq = fseq;
    }

    void set_global_block_seq(uint32_t gbseq)
    {
        _global_block_seq = gbseq;
    }

    void set_p2p_block_num(uint32_t bnum)
    {
        _p2p_block_num = bnum;
    }

    void set_timestamp(uint32_t i, uint64_t ts)
    {
        _timestamp[i] = ts;
    }

    void set_timestamp_num(uint32_t num)
    {
        _timestamp_num = num;
    }

    void set_rate_type(media_manager::RateType RateType)
    {
        _rt = RateType;
    }

    void set_content_len(uint32_t cl)
    {
        _content_len = cl;
    }

    void set_play_request_type(PlayRequest::PLAY_REQUEST_TYPE requesttype)
    {
        _request_type = requesttype;
    }

    StreamId_Ext get_sid() const
    {
        return _stream_id;
    }

    std::string get_token() const
    {
        return _token;
    }

    std::string get_method() const
    {
        return _method;
    }

    std::string get_path() const
    {
        return _path;
    }

    std::string get_query() const
    {
        return _query;
    }

    uint32_t get_fragment_seq() const
    {
        return _fragment_seq;
    }

    uint32_t get_global_block_seq_() const
    {
        return _global_block_seq;
    }

    uint32_t get_content_len() const
    {
        return _content_len;
    }

    uint32_t get_p2p_block_num()
    {
        return _p2p_block_num;
    }

    uint64_t get_timestamp(uint32_t i)
    {
        return _timestamp[i];
    }

    uint32_t get_timestamp_num()
    {
        return _timestamp_num;
    }

    media_manager::RateType get_rate_type() const
    {
        return _rt;
    }

    PlayRequest::PLAY_REQUEST_TYPE get_play_request_type() const
    {
        return _request_type;
    }

    bool set_quick_start(bool enable)
    {
        _quick_start = enable;
        return _quick_start;
    }

    bool get_quick_start(){ return _quick_start; }

    uint32_t set_pre_seek_ms(uint32_t offset)
    {
        _pre_seek_ms = offset;
        return _pre_seek_ms;
    }

    uint32_t get_pre_seek_ms(){ return _pre_seek_ms; }

    uint32_t set_speed_up(uint32_t speed_up)
    {
        _speed_up = speed_up;
        return speed_up;
    }

    uint32_t get_speed_up(){ return _speed_up; }

    bool set_key_frame_start(bool key_frame_start)
    {
        _key_frame_start = key_frame_start;
        return _key_frame_start;
    }

    bool get_key_frame_start(){ return _key_frame_start; }

public:
    uint8_t header_flag;

private:
    std::string                         _method;
    std::string                         _path;
    std::string                         _query;
    StreamId_Ext                        _stream_id;
    std::string                         _token;
    uint32_t                            _fragment_seq;
    uint32_t                            _global_block_seq;
    uint32_t                            _p2p_block_num;
    uint64_t                            _timestamp[10];
    uint32_t                            _timestamp_num;
    media_manager::RateType             _rt;
    PlayRequest::PLAY_REQUEST_TYPE      _request_type;

    uint32_t                            _content_len;
    bool                                _quick_start;
    uint32_t                            _pre_seek_ms;
    uint32_t                            _speed_up;
    bool                                _key_frame_start;
};

class BaseClient
{
public:
    BaseClient(struct event_base * main_base, void * bind)
        : c_(NULL),
        _main_base(main_base),
        _cmng(media_manager::CacheManager::get_player_cache_instance()),
        _bind(bind),
        _send_data(0),
        _send_data_interval(0)
    { 
    }
    virtual ~BaseClient() {}

    enum EatState
    {
        EAT_INIT = 0,
        EAT_HUNGRY = 1,
        EATING = 2
    };

    void set_connection(connection * c){ c_ = c; }
    void set_player_request(PlayRequest pr){ play_request_ = pr; }
    void set_last_block_seq(uint32_t seq){ last_block_seq_ = seq; }
    void set_update_time(time_t time){ update_time_ = time; }
    void set_byte_read(uint32_t br){ bytes_read_ = br; }
    void add_byte_read(uint32_t br) { bytes_read_ += br; }
    void set_player_stat(player_stat_t *player_stat) { _player_stat = player_stat; }

    uint32_t get_byte_read(){ return bytes_read_; }
    connection * get_connection(){ return c_; }
    PlayRequest & get_play_request(){ return play_request_; }
    uint32_t get_last_block_seq(){ return last_block_seq_; }
    long get_update_time() const
    {
        return update_time_;
    }
    void * get_player() { return _bind; }

    void enable_consume_data();
    void disable_consume_data();
    void enable_write(connection *c, void(*handler) (int fd, short which, void *arg));
    void disable_write(connection *c, void(*handler) (int fd, short which, void *arg));
    void switch_to_hungry();
    void switch_to_eat();
    int consume_data();

    virtual bool is_state_stop() = 0;
    virtual void set_init_stream_state() = 0;
    virtual void set_init_timeoffset(uint32_t ts) = 0;
    virtual void request_data() = 0;

    virtual void print_send_rate() {
        StreamId_Ext sid = play_request_.get_sid();

        INF("send to: %s, streamid: %s, tyep: %s, kbps: %d", c_->remote_ip, sid.c_str(), play_request_.type_to_string(), (int)(_send_data_interval*8.0 / 1024.0));
        _send_data_interval = 0;
    }

    EatState eat_state;

protected:
    connection * c_;
    PlayRequest play_request_;
    uint32_t bytes_read_;
    uint32_t last_block_seq_;
    long update_time_;
    struct event_base * _main_base;
	uint32_t timeoffset;

    player_stat_t *_player_stat;
    media_manager::PlayerCacheManagerInterface * _cmng;
    void * _bind;

    uint32_t _send_data; // bytes
    uint32_t _send_data_interval; // bytes
};
#endif
