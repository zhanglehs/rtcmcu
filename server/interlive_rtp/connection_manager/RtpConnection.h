#ifndef __CONNECTION_MANAGER_RTP_CONNECTION_H__
#define __CONNECTION_MANAGER_RTP_CONNECTION_H__

#include "streamid.h"
#include "utils/buffer.hpp"
#include <stdint.h>
#include <event.h>

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
  bool relay_pull;
  enConnectionType type;

  struct sockaddr_in remote;
  char remote_ip[32];
  struct in_addr local_ip;
  StreamId_Ext streamid;
  RTPTrans *trans;            // TODO: zhangle, need delete
  RtpManagerBase *manager;
  uint32_t audio_ssrc;
  uint32_t video_ssrc;

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

#endif /* __UPLOADER_H__ */
