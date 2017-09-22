/*******************************************
 * YueHonghui, 2013-06-08
 * hhyue@tudou.com
 * copyright:youku.com
 * ****************************************/
#ifndef MODULE_PLAYER_H_
#define MODULE_PLAYER_H_
#include <linux/limits.h>
#include <stdint.h>
#include <event.h>
#include <string>
#include "config_manager.h"

typedef struct player_stream player_stream;
struct session_manager;
class player_config : public ConfigModule
{
private:
    bool inited;

public:
    char player_listen_ip[32];
    uint16_t player_listen_port;
    int stuck_long_duration;
    int timeout_second;
    int max_players;
    size_t buffer_max;
    bool always_generate_hls;
    uint32_t ts_target_duration;
    uint32_t ts_segment_cnt;
    char private_key[64];
    char super_key[36];
    int sock_snd_buf_size;
    int normal_offset;
    int latency_offset;
    std::string crossdomain_path;
    char crossdomain[1024 * 16];
    size_t crossdomain_len;

public:
    player_config();
    virtual ~player_config();
    player_config& operator=(const player_config& rhv);
    virtual void set_default_config();
    virtual bool load_config(xmlnode* xml_config);
    virtual bool reload() const;
    virtual const char* module_name() const;
    virtual void dump_config() const;

private:
    bool load_config_unreloadable(xmlnode* xml_config);
    bool load_config_reloadable(xmlnode* xml_config);
    bool load_crossdomain_config();
    bool resove_config();
    bool parse_crossdomain_config();
};

typedef enum
{
    PLAYER_LIVE =0, //'s',
    PLAYER_AUDIO =1, //'a',
    PLAYER_TS =2, //'t',
    PLAYER_M3U8 =3, //'m',
    PLAYER_ORIG =4, //'o',
    PLAYER_CROSSDOMAIN = 5,
    PLAYER_V2_FRAGMENT =6,
    PLAYER_NA = 255,
} player_type; 

int player_init(struct event_base *main_base, struct session_manager *smng,
                const player_config * config);
uint32_t player_online_cnt(player_stream * ps);
void player_fini();

class Player;
Player* get_player_inst();

#endif
