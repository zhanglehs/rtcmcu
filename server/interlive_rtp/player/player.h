/***********************************************************************
* author: zhangcan
* email: zhangcan@youku.com
* date: 2015-02-13
* copyright:youku.com
************************************************************************/

#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "client.h"
#include "cache_manager.h"
#include "streamid.h"

struct connection;
typedef connection * connection_ptr;

namespace __gnu_cxx
{
    template<>
    struct hash<connection_ptr>
    {
    public:
        size_t operator()(const connection_ptr& key) const
        {
            size_t hash_num = CityHash64((char*)(&key), sizeof(key));
            return hash_num;
        }
    };
}

class Player {
public:
    Player(struct event_base *main_base, struct session_manager *smng,
        const player_config * config);
    ~Player();
    void add_play_task(connection *c, const char *lrcf2, const char *method, const char *path, const char *query_str);
    player_stat_t * get_player_stat();
    const player_stat_t * get_player_stat() const;
    void notify_stream_data(StreamId_Ext stream_id, uint8_t watch_type);
    void disconnect_client(BaseClient &client_connect_status);
    void start_timer();
    void check_timeout();
    void print_all_send_rate();
    int player_init();

private:
    void set_cache_manager(media_manager::PlayerCacheManagerInterface *cm);
    void set_player_config(const player_config * pconfig){ _config = pconfig; }
    void set_event_base(event_base * eb){ _main_base = eb; }
    void set_session_manager(struct session_manager* session_mgr){ _session_mgr = session_mgr; }
    void register_watcher();
    bool parser_query_str(const char *query_str, const char *path, PlayRequest& play_req);
    bool new_parser_query_str(const char *query_str, const char *path, PlayRequest& play_req);
    bool check_token(connection *c, const PlayRequest &play_req, const char *lrcf2);
    // get user_agent
    int get_ua(connection * c, const char *lrcf2, char **ua_r, size_t * ua_len);
private:
    struct event _ev_timer;
    time_t _time;
    const player_config * _config;
    struct session_manager* _session_mgr;
    struct event_base * _main_base;
    player_stat_t _player_stat;
    __gnu_cxx::hash_map<connection*, BaseClient*> _client_map;
    //__gnu_cxx::hash_map<int, connection*> connection_map;
    media_manager::PlayerCacheManagerInterface *_cmng;

    connection_ptr _conns_ptr;
};

#endif /* _PLAYER_H_ */
