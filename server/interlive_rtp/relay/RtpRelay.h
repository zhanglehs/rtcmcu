#ifndef _RELAY_RTP_RELAY_H_
#define _RELAY_RTP_RELAY_H_

#include "connection_manager/RtpConnectionManager.h"
#include <event.h>
#include <map>

class RtpPullClient;

class RtpPullTcpManager: public RtpTcpManager {
public:
  RtpPullTcpManager();
  ~RtpPullTcpManager();
  int Init(struct event_base * ev_base);
  void StartPull(const StreamId_Ext& streamid);
  void StopPull(const StreamId_Ext& streamid);

  virtual void OnConnectionClosed(RtpConnection *c);

protected:
  static void OnPullTimer(evutil_socket_t fd, short flag, void *arg);
  void OnPullTimerImpl(evutil_socket_t fd, short flag, void *arg);

  std::map<StreamId_Ext, RtpPullClient*> m_clients;
  struct event *m_ev_timer;
};

class RtpPushClient;
// TODO: zhangle, rtp失败后应重试
// pull处理得比较好了，但push还没有处理（主要是生命周期的管理和重试）
class RtpPushTcpManager : public RtpTcpManager {
public:
  int Init(struct event_base * ev_base);
  void StartPush(const StreamId_Ext& streamid);
  void StopPush(const StreamId_Ext& streamid);

protected:
  std::map<StreamId_Ext, RtpPushClient*> m_clients;
};

template <class C>
class SingletonBase {
public:
  static C* Instance();
  static void DestroyInstance();

protected:
  SingletonBase();
  static C* m_inst;
};

class TaskManger : public SingletonBase<TaskManger> {
public:
  TaskManger();
  ~TaskManger();

  void Init(struct event_base *ev_base);
  void PostTask(std::function<void()> task, unsigned int delay_ms = 0);

protected:
  static void OnTask(evutil_socket_t fd, short event, void *arg);
  void OnTaskImpl(evutil_socket_t fd, short event, void *arg);
  struct event_base *m_ev_base;
  std::set<struct event*> m_tasks;
  struct event* m_trash_task;
};

#endif
