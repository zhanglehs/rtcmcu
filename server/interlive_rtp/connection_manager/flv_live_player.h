#ifndef _CONNECTION_MANAGER_FLV_LIVE_PLAYER_H_
#define _CONNECTION_MANAGER_FLV_LIVE_PLAYER_H_

class LiveConnection;
class BaseLivePlayer {
public:
  BaseLivePlayer(LiveConnection *c);
  virtual ~BaseLivePlayer();
  virtual void OnWrite() = 0;

protected:
  LiveConnection *m_connection;
};

namespace media_manager {
  class FlvCacheManager;
}
class FlvLivePlayer : public BaseLivePlayer {
public:
  FlvLivePlayer(LiveConnection *c);
  virtual void OnWrite();

protected:
  bool m_written_header;
  bool m_written_first_tag;

  bool m_buffer_overuse;
  int m_latest_blockid;
  media_manager::FlvCacheManager * _cmng;
};

class player_config;
class CrossdomainLivePlayer : public BaseLivePlayer {
public:
  CrossdomainLivePlayer(LiveConnection *c, const player_config *config);
  virtual void OnWrite();

protected:
  const player_config *m_config;
};

#endif /* _CLIENT_H_ */
