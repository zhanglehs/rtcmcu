#ifndef __UPLOADER_H__
#define __UPLOADER_H__

#include "connection_manager/RtpConnection.h"
#include "network/rtp_send_queue.h"
#include <set>
#include <map>

namespace avformat {
  class RtcpPacket;
}

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

protected:
  static void OnSocketAccept(const int fd, const short which, void *arg);
  void OnSocketAcceptImpl(const int fd, const short which);

  LibEventSocket m_ev_socket;
  static RtpTcpServerManager* m_inst;
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
