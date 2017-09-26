#ifndef __UPLOADER_H__
#define __UPLOADER_H__

#include <stdint.h>
#include <event.h>
#include <time.h>
#include "streamid.h"
#include <json.h>
#include "utils/buffer.hpp"
#include "target.h"
#include "portal.h"
#include "util/flv.h"
#include "config_manager.h"
#include "media_manager/cache_manager.h"
#include "uploader_config.h"
#include <set>
#include <list>
#include "whitelist_manager.h"
//#include "uploader/RtpTcpConnectionManager.h"
#include "avformat/rtcp.h"
#include "transport/rtp_send_queue.h"

typedef void(*LibEventCb)(const int, const short, void *);

struct LibEventSocket {
  struct event_base* ev_base;
  int fd;
  LibEventCb cb;
  void *cb_arg;
  struct event ev_socket;
  void Start(struct event_base* ev_base, int fd, LibEventCb cb, void *cb_arg);
  void Stop();
  void EnableWrite();
  void DisableWrite();
  bool w_enabled;
};

class RTPTrans;
class RtpManagerBase;

class RtpConnection {
public:
  enum enConnectionType {
    CONN_TYPE_UNKOWN = 0,
    CONN_TYPE_PLAYER,
    CONN_TYPE_UPLOADER,
  };
  RtpConnection();
  ~RtpConnection();

  bool udp;
  enConnectionType type;

  struct sockaddr_in remote;
  char remote_ip[32];
  struct in_addr local_ip;
  StreamId_Ext streamid;
  RTPTrans *trans;            // TODO: zhangle, need delete
  RtpManagerBase *manager;
  uint32_t audio_ssrc;
  uint32_t video_ssrc;
  media_manager::RTPMediaCache *rtp_cache;

  uint32_t send_bytes;
  uint32_t recv_bytes;
  uint32_t send_bytes_by_min; // bytes sended by player by minute
  uint32_t recv_bytes_by_min; // bytes received by uploader by minute

  bool IsPlayer();
  bool IsUploader();
  void EnableWrite();
  void DisableWrite();
  static void Destroy(RtpConnection *c);

  // used by tcp
  LibEventSocket ev_socket;
  buffer *rb;
  buffer *wb;
  unsigned int create_time;
};

class RTPTransManager;
class RTPMediaManagerHelper;

class RtpManagerBase {
public:
  int SendRtcp(RtpConnection *c, const avformat::RtcpPacket *rtcp);
  int SendRtp(RtpConnection *c, const unsigned char *rtp, int len, uint32_t ssrc = 0);
  int SendData(RtpConnection *c, const unsigned char *data, int len); // rtp发送数据的总入口

  int OnReadPacket(RtpConnection *c, buffer *buf);   // rtp接收数据的总入口

  virtual void OnConnectionClosed(RtpConnection *c) = 0;

protected:
  RtpManagerBase();
  virtual ~RtpManagerBase();

protected:
public:
  static RTPTransManager *m_trans_mgr;
  static RTPMediaManagerHelper *m_rtp_mmh;
  struct event_base* m_ev_base;
};

class RtpTcpManager : public RtpManagerBase {
public:
  RtpConnection* CreateConnection(struct sockaddr_in *remote, int socket);
  virtual void OnConnectionClosed(RtpConnection *c);

protected:
  RtpTcpManager();
  ~RtpTcpManager();
  static void OnSocketData(const int fd, const short which, void *arg);
  void OnSocketDataImpl(RtpConnection *c, const short which);

  void StartTimer();
  static void OnTimer(const int fd, short which, void *arg);
  void OnTimerImpl(const int fd, short which);

  struct event m_ev_timer;

  // 临时连接管理。在交给RTPTransManager之前，若RtpConnection超时，需要收尸
  std::set<RtpConnection*> m_connections;
};

class RtpTcpServerManager : public RtpTcpManager {
public:
  static RtpTcpServerManager* Instance();
  static void DestroyInstance();

  int32_t Init(struct event_base *ev_base);

  int set_http_server(http::HTTPServer *http_server); // rtp http server的总入口

protected:
  static void OnSocketAccept(const int fd, const short which, void *arg);
  void OnSocketAcceptImpl(const int fd, const short which);

  static void http_put_sdp(http::HTTPConnection*, void*);
  static void http_put_sdp_check(http::HTTPConnection*, void*);
  static void get_count_handler(http::HTTPConnection* conn, void* args);
  static void get_count_check_handler(http::HTTPConnection* conn, void* args);
  static void http_get_sdp_handle(http::HTTPConnection* conn, void* args);
  static void http_get_sdp_check_handle(http::HTTPConnection* conn, void* args);

  void start_timer();
  static void timer_cb(const int fd, short which, void *arg);

  LibEventSocket m_ev_socket;
  static RtpTcpServerManager* m_inst;
  struct event m_ev_timer;
};

class RtpUdpServerManager : public RtpManagerBase {
public:
  static RtpUdpServerManager* Instance();
  static void DestroyInstance();

  int Init(struct event_base *ev_base);
  void EnableWrite();
  void DisableWrite();
  int SendUdpData(RtpConnection *c, const unsigned char *data, int len);
  virtual void OnConnectionClosed(RtpConnection *c);

protected:
  RtpUdpServerManager();
  ~RtpUdpServerManager();
  static void OnSocketData(const int fd, const short which, void *arg);
  void OnSocketDataImpl(const int fd, const short which);
  void OnRead();
  void OnWrite();

  buffer *m_recv_buf;
  LibEventSocket m_ev_socket;

  RTPSendQueue m_send_queue;

  class RtpConnectionIdLessThan {
  public:
    bool operator() (const struct sockaddr_in& s1,
      const struct sockaddr_in& s2) const {
      return memcmp(&s1, &s2, sizeof(struct sockaddr_in)) > 0;
    }
  };
  std::map<struct sockaddr_in, RtpConnection*, RtpConnectionIdLessThan> m_connections;
  static RtpUdpServerManager* m_inst;
};

#endif /* __UPLOADER_H__ */
