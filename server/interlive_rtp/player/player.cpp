/************************************************************************
* author: zhangcan
* email: zhangcan@youku.com
* date: 2014-12-30
* copyright:youku.com
************************************************************************/

#include "player.h"
#include "util/util.h"
#include "util/log.h"
#include "util/access.h"
#include "util/levent.h"
#include "util/session.h"
#include "utils/memory.h"
#include "fragment/fragment.h"
#include "connection.h"
//#include "target.h"
#include "whitelist_manager.h"
#include <appframe/singleton.hpp>

#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "../target_config.h"
#include "perf.h"

using namespace std;
using namespace __gnu_cxx;
using namespace fragment;
using namespace media_manager;

#define PLAYER_TOKEN_TIMEOUT   24 * 3600
#define STREAM_ID_LENGTH       32

static void timer_service(const int fd, short which, void *arg)
{
    Player *pm = (Player *)arg;
    pm->check_timeout();
    if (time(NULL) % 3 == 0)
    {
        pm->print_all_send_rate();
    }
    pm->start_timer();
}

static void player_notify_stream_data(StreamId_Ext stream_id, uint8_t watch_type, void* arg)
{
    assert(arg != NULL);
    Player * player = (Player*)arg;
    player->notify_stream_data(stream_id, watch_type);
}

Player::Player(struct event_base *main_base, struct session_manager *smng,
    const player_config * config)
{
    _time = time(NULL); 
    set_cache_manager(media_manager::CacheManager::get_player_cache_instance());
    set_event_base(main_base);
    set_player_config(config);
    set_session_manager(smng);
    start_timer();
}

Player::~Player()
{

}

// TODO
// replace init method v1
int Player::player_init()
{
    if (NULL == _main_base || NULL == _config) {
        ERR("main_base or config is NULL. main_base = %p, config = %p",
            _main_base, _config);
        return -1;
    }
    //g_time = time(NULL);
    int i = 0;
    int val = 0, ret = 0;
    struct sockaddr_in addr;

    int player_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == player_listen_fd) {
        ERR("socket failed. error = %s", strerror(errno));
        return -2;
    }

    val = 1;
    if (-1 == setsockopt(player_listen_fd, SOL_SOCKET, SO_REUSEADDR,
        (void *)&val, sizeof(val))) {
        ERR("set socket SO_REUSEADDR failed. error = %s", strerror(errno));
        return -5;
    }

    /*if (0 != set_player_sockopt(player_listen_fd)) {
        ERR("set player_listen_fd sockopt failed.");
        return -6;
    }*/
#ifdef TCP_DEFER_ACCEPT
    {
        int v = 30;             /* 30 seconds */

        if (-1 ==
            setsockopt(player_listen_fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &v,
            sizeof(v))) {
            WRN("can't set TCP_DEFER_ACCEPT on player_listen_fd %d",
                player_listen_fd);
        }
    }
#endif

    addr.sin_family = AF_INET;
    inet_aton("0.0.0.0", &(addr.sin_addr));
    addr.sin_port = htons(_config->player_listen_port);

    ret = bind(player_listen_fd, (struct sockaddr *) &addr, sizeof(addr));
    if (0 != ret) {
        ERR("bind addr failed. error = %s", strerror(errno));
        return -8;
    }

    ret = listen(player_listen_fd, 5120);
    if (0 != ret) {
        ERR("listen player fd failed. error = %s", strerror(errno));
        return -9;
    }

    _conns_ptr =
        (connection *)mcalloc(_config->max_players, sizeof(connection));
    if (NULL == _conns_ptr) {
        ERR("g_conns mcalloc failed.");
        close(player_listen_fd);
        return -10;
    }

    for (i = 0; i < _config->max_players; i++) {
        _conns_ptr[i].fd = -1;
    }

    pid_t pid = getpid();
    srand(pid);    /* init random seed */
    struct event ev_listener;
    levent_set(&ev_listener, player_listen_fd, EV_READ | EV_PERSIST,
        NULL, NULL);
    event_base_set(_main_base, &ev_listener);
    levent_add(&ev_listener, 0);

    return 0;
}

void Player::start_timer()
{
    struct timeval tv;
    levtimer_set(&_ev_timer, timer_service, (void *)this);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    event_base_set(_main_base, &_ev_timer);
    levtimer_add(&_ev_timer, &tv);
}

void Player::check_timeout()
{
    time_t now = time(NULL);
    _time = now;

    hash_map<connection*, BaseClient*>::iterator iter = _client_map.begin();
    vector<BaseClient*> status_vec;

    for (; iter != _client_map.end(); iter++)
    {
        BaseClient* ccs = iter->second;
        TRC("check time out now=%ld ccs.last_update=%ld\t config time_out=%d", now, ccs->get_update_time(), _config->timeout_second);
        if (now - ccs->get_update_time() > _config->timeout_second)
        {
            status_vec.push_back(ccs);
        }
    }

    for (vector<BaseClient*>::iterator it = status_vec.begin(); it != status_vec.end(); it++)
    {
        disconnect_client(*(*it));
    }

    TRC("after check time out client size = %d", (int)_client_map.size());
}

void Player::print_all_send_rate()
{
    hash_map<connection*, BaseClient*>::iterator iter = _client_map.begin();

    for (; iter != _client_map.end(); iter++)
    {
        BaseClient* ccs = iter->second;
        ccs->print_send_rate();
    }
}

player_stat_t * Player::get_player_stat()
{
    return &_player_stat;
}

const player_stat_t * Player::get_player_stat() const
{
    return &_player_stat;//get_player_stat();
}

bool Player::parser_query_str(const char *query_str, const char *path, PlayRequest& play_req)
{
    char buf[64];

    // check clarity
    if (!util_query_str_get(query_str, strlen(query_str), "c", buf, sizeof(buf)))
    {
        play_req.set_rate_type(media_manager::RATE_COMPLETE);
    }
    else
    {
        std::string cr(buf);
        if (cr == "HD")
        {
            play_req.set_rate_type(media_manager::RATE_COMPLETE);
        }
        if (cr == "SD")
        {
            play_req.set_rate_type(media_manager::RATE_KEY_VIDEO_AND_AUDIO);
        }
        if (cr == "VO")
        {
            play_req.set_rate_type(media_manager::RATE_AUDIO);
        }
    }
    memset(buf, 0, sizeof(buf));

    // check token
    if (!util_query_str_get(query_str, strlen(query_str), "a", buf, sizeof(buf)))
    {
        return false;
    }
    else
    {
        std::string tok(buf);
        play_req.set_token(tok);
    }

    // check stream id
    memset(buf, 0, sizeof(buf));
    if (!util_query_str_get(query_str, strlen(query_str), "b", buf, sizeof(buf)))
    {
        return false;
    }
    else
    {
        StreamId_Ext stream_id;
        if (stream_id.parse(buf, 16) == 0)
        {
            play_req.set_sid(stream_id);
        }
        else
        {
            return false;
        }
    }

    // check path
    memset(buf, 0, sizeof(buf));
    switch (path[4])
    {
    case 'o':
        if (!util_query_str_get(query_str, strlen(query_str), "fn", buf, sizeof(buf)))
        {
            memset(buf, 0, sizeof(buf));
            if (!util_query_str_get(query_str, strlen(query_str), "bn", buf, sizeof(buf)))
            {
                play_req.set_play_request_type(PlayRequest::RequestLive);
                return true;
            }
            else
            {
                if (util_check_digit(buf, strlen(buf)))
                {
                    uint32_t global_block_seq = strtol(buf, (char **)NULL, 10);
                    play_req.set_global_block_seq(global_block_seq);
                    play_req.set_play_request_type(PlayRequest::RequestBlock);
                    return true;
                }
                else
                {
                    play_req.set_play_request_type(PlayRequest::RequestInvalid);
                    return false;
                }
            }
        }
        else
        {
            if (util_check_digit(buf, strlen(buf)))
            {
                uint32_t fragment_seq = strtol(buf, (char **)NULL, 10);
                play_req.set_fragment_seq(fragment_seq);
                play_req.set_play_request_type(PlayRequest::RequestFragment);
                return true;
            }
            else
            {
                play_req.set_play_request_type(PlayRequest::RequestInvalid);
                return false;
            }
        }
        break;

    case 'f':
        if (util_query_str_get(query_str, strlen(query_str), "fn", buf, sizeof(buf)))
        {
            if (util_check_digit(buf, strlen(buf)))
            {
                uint32_t fragment_seq = strtol(buf, (char **)NULL, 10);
                play_req.set_fragment_seq(fragment_seq);
                play_req.set_play_request_type(PlayRequest::RequestFragment);
                return true;
            }
            else
            {
                play_req.set_play_request_type(PlayRequest::RequestInvalid);
                return false;
            }
        }
        break;
    case 't':
        if (util_query_str_get(query_str, strlen(query_str), "idx", buf, sizeof(buf)))
        {
            if (util_check_digit(buf, strlen(buf)))
            {
                uint32_t fragment_seq = strtol(buf, (char **)NULL, 10);
                play_req.set_fragment_seq(fragment_seq);
                play_req.set_play_request_type(PlayRequest::RequestHLSTSSegment);
                return true;
            }
            else
            {
                play_req.set_play_request_type(PlayRequest::RequestInvalid);
                return false;
            }
        }
        break;
    case 'm':
        play_req.set_play_request_type(PlayRequest::RequestHLSM3U8);
        return true;
        break;
    case 'a':
    {
                memset(buf, 0, sizeof(buf));
                if (!util_query_str_get(query_str, strlen(query_str), "bn", buf, sizeof(buf)))
                {
                    play_req.set_play_request_type(PlayRequest::RequestLiveAudio);
                    return true;
                }
                else
                {
                    if (util_check_digit(buf, strlen(buf)))
                    {
                        uint32_t global_block_seq = strtol(buf, (char **)NULL, 10);
                        play_req.set_global_block_seq(global_block_seq);
                        play_req.set_play_request_type(PlayRequest::RequestBlock);
                        return true;
                    }
                    else
                    {
                        play_req.set_play_request_type(PlayRequest::RequestInvalid);
                        return false;
                    }
                }
    }

    default:
        break;
    }

    return true;
}

bool Player::new_parser_query_str(const char *query_str, const char *path, PlayRequest& play_req)
{
    char buf[64];
    memset(buf, 0, sizeof(buf));

    // get token
    if (!util_query_str_get(query_str, strlen(query_str), "token", buf, sizeof(buf)))
    {
        return false;
    }
    std::string tok(buf);
    play_req.set_token(tok);

    // get stream id and request type
    memset(buf, 0, sizeof(buf));

    if (strstr(path, "/live/nodelay/"))
    {
        if (!util_path_str_get(path, strlen(path), 4, buf, 64))
        {
            ERR("wrong path, cannot get stream id.");
            return false;
        }
        buf[STREAM_ID_LENGTH] = '\0';
        StreamId_Ext stream_id;
        if (!util_check_hex(buf, 32) || (stream_id.parse(buf, 16) != 0))
        {
            return false;
        }
        play_req.set_sid(stream_id);
        play_req.set_play_request_type(PlayRequest::RequestFlvMiniBlock);

        if (strstr(path, ".audio.flv") || strstr(path, "/audio")){
            play_req.header_flag = FLV_FLAG_AUDIO;
        }
        if (strstr(path, ".video.flv") || strstr(path, "/video")){
            play_req.header_flag = FLV_FLAG_VIDEO;
        }
        return true;
    }
    else if (strstr(path, "/live/flv/") || strstr(path, "/live/f/"))
    {
        if (!util_path_str_get(path, strlen(path), 4, buf, 64))
        {
            ERR("wrong path, cannot get stream id.");
            return false;
        }
        buf[STREAM_ID_LENGTH] = '\0';
        StreamId_Ext stream_id;
        if (!util_check_hex(buf, 32) || (stream_id.parse(buf, 16) != 0))
        {
            return false;
        }
        play_req.set_sid(stream_id);

        if (strstr(path, ".audio.flv") || strstr(path, "/audio"))
        {
            play_req.set_play_request_type(PlayRequest::RequestLiveAudio);
        }
        else
        {
            util_query_str_get(query_str, strlen(query_str), "quick_start", buf, sizeof(buf));
            if (util_check_digit(buf, strlen(buf)))
            {
                uint32_t quick_start = strtol(buf, (char **)NULL, 10);
                play_req.set_quick_start(quick_start == 1);
            }

            util_query_str_get(query_str, strlen(query_str), "pre_seek_ms", buf, sizeof(buf));
            if (util_check_digit(buf, strlen(buf)))
            {
                uint32_t pre_seek_ms = strtol(buf, (char **)NULL, 10);
                play_req.set_pre_seek_ms(pre_seek_ms);
            }

            util_query_str_get(query_str, strlen(query_str), "speed_up", buf, sizeof(buf));
            if (util_check_digit(buf, strlen(buf)))
            {
                uint32_t speed_up = strtol(buf, (char **)NULL, 10);
                play_req.set_speed_up(speed_up);
            }

            util_query_str_get(query_str, strlen(query_str), "non_key_frame_start", buf, sizeof(buf));
            if (util_check_digit(buf, strlen(buf)))
            {
                uint32_t non_key_frame_start = strtol(buf, (char **)NULL, 10);
                play_req.set_key_frame_start(non_key_frame_start == 1 ? false :true);
            }

            play_req.set_play_request_type(PlayRequest::RequestLive);
        }
        return true;
    }
    else if (strstr(path, "/hls/"))
    {
        if (!util_path_str_get(path, strlen(path), 4, buf, 64))
        {
            ERR("wrong url path, cannot get stream id.");
            return false;
        }
        buf[STREAM_ID_LENGTH] = '\0';
        StreamId_Ext stream_id;
        if (!util_check_hex(buf, 32) || (stream_id.parse(buf, 16) != 0))
        {
            return false;
        }
        play_req.set_sid(stream_id);

        if (strstr(path, ".m3u8"))
        {
            play_req.set_play_request_type(PlayRequest::RequestHLSM3U8);
        }
        else if (strstr(path, ".ts"))
        {
            play_req.set_play_request_type(PlayRequest::RequestHLSTSSegment);
            memset(buf, 0, sizeof(buf));
            if (!util_path_str_get(path, strlen(path), 5, buf, 64))
            {
                ERR("wrong url path, cannot ts segment index.");
                return false;
            }

            int ts_pos = strchr(buf, '.') - buf;
            buf[ts_pos] = '\0';
            if (util_check_digit(buf, strlen(buf)))
            {
                uint32_t fragment_seq = strtol(buf, (char **)NULL, 10);
                play_req.set_fragment_seq(fragment_seq);
            }
        }
        return true;
    }
    else if (strstr(path, "/flv-fragment/"))
    {
        if (!util_path_str_get(path, strlen(path), 4, buf, 64))
        {
            ERR("wrong path, cannot get stream id.");
            return false;
        }
        buf[STREAM_ID_LENGTH] = '\0';
        StreamId_Ext stream_id;
        if (!util_check_hex(buf, 32) || (stream_id.parse(buf, 16) != 0))
        {
            return false;
        }
        play_req.set_sid(stream_id);

        if (strstr(path, ".flv"))
        {
            play_req.set_play_request_type(PlayRequest::RequestFragment);
        }
        return true;
    }
    else if (strstr(path, "/flvfrag"))
    {
        if (!util_query_str_get(query_str, strlen(query_str), "sid", buf, sizeof(buf)))
        {
            ERR("parse error, cannot find para sid.");
            return false;
        }
        StreamId_Ext stream_id;
        if (stream_id.parse(buf, 16) != 0)
        {
            return false;
        }
        play_req.set_sid(stream_id);

        if (strstr(path, "onmetadata"))
        {
            play_req.set_play_request_type(PlayRequest::RequestP2PMetadata);
        }
        else if (strstr(path, "timerange"))
        {
            play_req.set_play_request_type(PlayRequest::RequestP2PTimerange);
        }
        else if (strstr(path, "random"))
        {
            // get timestamp
            memset(buf, 0, sizeof(buf));
            if (!util_query_str_get(query_str, strlen(query_str), "st", buf, sizeof(buf)))
            {
                ERR("parse error, cannot find para st.");
                return false;
            }
            int in = 0;
            char *p[10] = { 0 };
            char *tmp_buf = buf;
            while ((p[in] = strtok(tmp_buf, ",")) != NULL && in < 10) {
                in++;
                tmp_buf = NULL;
            }
            uint32_t timestamp_num = 0;
            for (uint32_t i = 0; i < 10; i++) {
                if (p[i] != NULL) {
                    if (!util_check_digit(p[i], strlen(p[i])))
                    {
                        return false;
                    }
                    uint64_t p2p_timestamp = strtol(p[i], (char **)NULL, 10);
                    play_req.set_timestamp(i, p2p_timestamp);
                    timestamp_num++;
                }
            }
            play_req.set_timestamp_num(timestamp_num);
            play_req.set_play_request_type(PlayRequest::RequestP2PRandomSegment);
        }
        else
        {
            // get block number
            memset(buf, 0, sizeof(buf));
            if (!util_query_str_get(query_str, strlen(query_str), "bn", buf, sizeof(buf)))
            {
                ERR("parse error, cannot find para bn.");
                return false;
            }
            if (util_check_digit(buf, strlen(buf)))
            {
                uint32_t p2p_block_num = strtol(buf, (char **)NULL, 10);
                play_req.set_p2p_block_num(p2p_block_num);
            }
            //get timestamp
            memset(buf, 0, sizeof(buf));
            if (!util_query_str_get(query_str, strlen(query_str), "st", buf, sizeof(buf)))
            {
                ERR("parse error, cannot find para st.");
                return false;
            }
            if (util_check_digit(buf, strlen(buf)))
            {
                uint64_t p2p_timestamp = strtol(buf, (char **)NULL, 10);
                play_req.set_timestamp(0, p2p_timestamp);
            }
            play_req.set_play_request_type(PlayRequest::RequestP2PSegment);
        }
        return true;
    }
    else if (strstr(path, "/download/sdp"))
    {
        TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");
        if (!common_config->enable_rtp)
        {
            return false;
        }
        if (!util_path_str_get(path, strlen(path), 3, buf, 64))
        {
            ERR("wrong path, cannot get stream id.");
            return false;
        }
        buf[STREAM_ID_LENGTH] = '\0';
        StreamId_Ext stream_id;
        if (!util_check_hex(buf, 32) || (stream_id.parse(buf, 16) != 0))
        {
            return false;
        }

        play_req.set_sid(stream_id);
        play_req.set_play_request_type(PlayRequest::RequestSDP);

        return true;
    }
    else
    {
        play_req.set_play_request_type(PlayRequest::RequestInvalid);
        return false;
    }
}

void Player::add_play_task(connection *c, const char *lrcf2, const char *method, const char *path, const char *query_str)
{
    PlayRequest play_req;
    play_req.set_method(method);
    play_req.set_path(path);
    play_req.set_query(query_str);
    INF("play query_str is %s", query_str);
    // for compitability, we still need to keep the old parse logic
    if (!parser_query_str(query_str, path, play_req))
    {
        if (!new_parser_query_str(query_str, path, play_req))
        {
            util_http_rsp_error_code(c->fd, HTTP_404);
            conn_close(c);
            return;
        }
    }

    StreamId_Ext id = play_req.get_sid();

    if (!check_token(c, play_req, lrcf2))
    {
        ACCESS("player-live %s:%6hu %s %s?%s 403", c->remote_ip, c->remote_port, method, path, query_str);
        util_http_rsp_error_code(c->fd, HTTP_404);
        conn_close(c);
        return;
    }

    //if (!stream_in_whitelist(id))
    if (!SINGLETON(WhitelistManager)->in(id))
    {
        ACCESS("player-live %s:%6hu %s %s?%s 404", c->remote_ip, c->remote_port, method, path, query_str);
        util_http_rsp_error_code(c->fd, HTTP_404);
        conn_close(c);
        return;
    }

    if (Perf::get_instance()->is_sys_busy())
    {
        ACCESS("player-live %s:%6hu %s %s?%s 509", c->remote_ip, c->remote_port, method, path, query_str);
        util_http_rsp_error_code(c->fd, HTTP_509);
        conn_close(c);
        return;
    }


    // TODO 
    // implement factory class to create different client object,
    // in factory class, multiplexing objects
    BaseClient *client_connect = NULL;
    switch (play_req.get_play_request_type())
    {
    case PlayRequest::RequestFlvMiniBlock:
        client_connect = new FLVMiniBlockClient(_main_base, (void*)this);
        break;
    case PlayRequest::RequestSDP:
        client_connect = new SDPClient(_main_base, (void *)this);
        break;
    default:
        return;
    }

    client_connect->set_connection(c);
    DBG("add task connection is %p", c);
	client_connect->set_init_timeoffset(0);
    client_connect->set_init_stream_state();
    session_manager_detach(&c->timer);
    client_connect->set_player_request(play_req);
    _client_map[c] = client_connect;
    //connection_map[c->fd] = c;
    c->bind = (void *)client_connect;
    _player_stat.add_online_cnt();
    client_connect->enable_consume_data();
}

void Player::disconnect_client(BaseClient &client_connect_status)
{
    //vector<BaseClient>::iterator iter;
    DBG("disconnect connection c=%p", client_connect_status.get_connection());

    connection *c = client_connect_status.get_connection();
    if (NULL != _client_map[c])
        delete _client_map[c];
    _client_map.erase(c);

    _player_stat.minus_online_cnt();
    conn_close(c);
}

void Player::set_cache_manager(media_manager::PlayerCacheManagerInterface *cm)
{
    _cmng = cm;
    register_watcher();
}

void Player::register_watcher()
{
    if (_cmng != NULL)
    {
        cache_watch_handler fobj = player_notify_stream_data;
        _cmng->register_watcher(fobj, media_manager::CACHE_WATCHING_ALL, this);
    }
}

void Player::notify_stream_data(StreamId_Ext stream_id, uint8_t watch_type)
{
    if(_client_map.empty()) {
        return;
    }
    __gnu_cxx::hash_map<connection*, BaseClient* > ::iterator iter = _client_map.begin();

    for (; iter != _client_map.end(); iter++)
    {
        BaseClient* status = iter->second;
        if (status->eat_state != BaseClient::EAT_HUNGRY)
        {
            continue;
        }

        if (status->get_play_request().get_sid() == stream_id)
        {
            status->enable_consume_data();
        }
    }
}

bool Player::check_token(connection *c, const PlayRequest &play_req, const char *lrcf2)
{
    char token[64] = { 0 };
    uint32_t streamid = play_req.get_sid().get_32bit_stream_id();
    strcpy(token, play_req.get_token().c_str());

    char *ua = NULL;
    size_t ua_len = 0;
    int ret = -1;
    ret = get_ua(c, lrcf2, &ua, &ua_len);
    if (0 != ret)
    {
        WRN("player-live ua get failed. ret = %d", ret);
        return false;
    }

    if (!util_check_token(_time, streamid, _config->private_key, strlen(_config->private_key), ua, ua_len, c->local_ip,
        strlen(c->local_ip), PLAYER_TOKEN_TIMEOUT, token) && 0 != strcmp(token, _config->super_key))
    {
        WRN("player-live check token failed. #%d(%s:%hu), time = %ld, streamid = %u," " private_key = %s, ua = %s, local_addr = %s:%hu",
            c->fd, c->remote_ip, c->remote_port, (int64_t)_time, streamid, _config->private_key, ua, c->local_ip, c->local_port);
        return false;
    }

    return true;
}

char empty_ua[] = "";

// get user_agent
int Player::get_ua(connection * c, const char *lrcf2, char **ua_r, size_t * ua_len)
{
    int lrcf2_l = lrcf2 - (char *)buffer_data_ptr(c->r);
    *ua_len = 0;
    size_t ua_key_l = strlen("\r\nUser-Agent:");
    char *ua_tmp = (char *)memmem(buffer_data_ptr(c->r), lrcf2_l, "\r\nUser-Agent:", ua_key_l);

    if (NULL != ua_tmp)
    {
        ua_tmp += ua_key_l;
        char *ua_end = (char *)memmem(ua_tmp, lrcf2 - ua_tmp + 4, "\r\n", 2);

        if (NULL == ua_end)
        {
            return 1;
        }
        ua_end--;
        while (ua_tmp <= ua_end && ' ' == *ua_tmp)
            ua_tmp++;
        while (ua_end >= ua_tmp && ' ' == *ua_end)
            ua_end--;
        if (ua_tmp <= ua_end)
        {
            *ua_len = ua_end - ua_tmp + 1;
        }
        else
        {
            DBG("ua is empty or space...#%d(%s:%hu)",
                c->fd, c->remote_ip, c->remote_port);
            *ua_len = 0;
        }
        *(ua_tmp + *ua_len) = 0;
    }

    *ua_r = ua_tmp;
    if (*ua_r == NULL)
    {
        WRN("ua is null, remote_ip: %s, query: %s", c->remote_ip, buffer_data_ptr(c->r));
        *ua_r = empty_ua;
    }
    return 0;
}
