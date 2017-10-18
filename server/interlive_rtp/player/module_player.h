#ifndef MODULE_PLAYER_H_
#define MODULE_PLAYER_H_

#include "streamid.h"
#include "uploader/RtpTcpConnectionManager.h"
#include "config_manager.h"
#include <linux/limits.h>
#include <stdint.h>
#include <string>

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

//////////////////////////////////////////////////////////////////////////

class BaseLivePlayer;
class LiveConnection {
public:
  LiveConnection();
  ~LiveConnection();

  struct sockaddr_in remote;
  char remote_ip[32];
  struct in_addr local_ip;
  LibEventSocket ev_socket;
  buffer *rb;
  buffer *wb;
  void EnableWrite();
  void DisableWrite();
  static void Destroy(LiveConnection *c);
  time_t active_time_s;
  bool async_close;

  BaseLivePlayer *live;
  StreamId_Ext streamid;
};

// INFO: zhangle, flv live play url like this:
// http://192.168.245.133:8089/live/nodelay/v1/00000000000000000000000015958F05?token=98765
class player_config;
class LiveConnectionManager {
public:
  static LiveConnectionManager* Instance();
  static void DestroyInstance();

  int Init(struct event_base *ev_base, const player_config * config);

  void OnConnectionClosed(LiveConnection *c);

  bool HasPlayer(const StreamId_Ext &streamid);

protected:
  static void OnSocketAccept(const int fd, const short which, void *arg);
  void OnSocketAcceptImpl(const int fd, const short which);
  static void OnSocketData(const int fd, const short which, void *arg);
  void OnSocketDataImpl(LiveConnection *c, const short which);
  static void OnRecvStreamData(StreamId_Ext stream_id, uint8_t watch_type, void* arg);
  void OnRecvStreamDataImpl(StreamId_Ext stream_id, uint8_t watch_type);
  void StartTimer();
  static void OnTimer(const int fd, short which, void *arg);
  void OnTimerImpl();

  struct event_base *m_ev_base;
  LibEventSocket m_ev_socket;
  struct event m_ev_timer;
  const player_config *m_config;
  std::map<uint32_t, std::set<LiveConnection*> > m_stream_groups;
  std::set<LiveConnection*> m_connections;

  static LiveConnectionManager* m_inst;
};

#endif
