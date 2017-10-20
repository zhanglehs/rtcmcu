#ifndef _RELAY_RELAY_MANAGER_H_
#define _RELAY_RELAY_MANAGER_H_

struct event_base;
class StreamId_Ext;
class RtpPushTcpManager;
class RtpPullTcpManager;

class RelayManager {
public:
  static RelayManager* Instance();
  static void DestroyInstance();

  int Init(struct event_base *ev_base);

  int StartPullRtp(const StreamId_Ext& stream_id);
  int StopPullRtp(const StreamId_Ext& stream_id);
  int StartPushRtp(const StreamId_Ext& stream_id);
  int StopPushRtp(const StreamId_Ext& stream_id);

protected:
  RelayManager();
  ~RelayManager();

  RtpPushTcpManager *m_push_manager;
  RtpPullTcpManager *m_pull_manager;
  static RelayManager* m_inst;
};

#endif
