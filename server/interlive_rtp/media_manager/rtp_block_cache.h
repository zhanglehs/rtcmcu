/*
* RtpCacheManager: 是rtp缓存管理的单例类
* RtpCacheManager、RTPMediaCache、RTPCircularCache的关系为：
*     RtpCacheManager = map<StreamId_Ext, RTPMediaCache*>;
*     RTPMediaCache = sdp + RTPCircularCache(audio) + RTPCircularCache(video)
*     RTPMediaCache代表了一路流的缓存信息，包含sdp、audio缓存、video缓存；
*     RTPCircularCache实现了audio/video的循环缓存；
*/

#pragma once

#include "fragment/rtp_block.h"
#include "streamid.h"
#include <map>
#include <set>

namespace media_manager {
  class RTPMediaCache;
}

// TODO: zhangle, 如果一路流很久没有收到数据，应从cache中删除
class RtpCacheManager {
public:
  static RtpCacheManager* Instance();
  static void DestroyInstance();

  int Init(struct event_base *ev_base);
  //class RtpCacheWatcher {
  //public:
  //  virtual ~RtpCacheWatcher() {}
  //  virtual void OnRtp() {}
  //  virtual void OnSdp() {}
  //};
  //void AddWatcher(RtpCacheWatcher *watcher);
  //void RemoveWatcher(RtpCacheWatcher *watcher);

  int set_rtp(const StreamId_Ext& stream_id, const avformat::RTP_FIXED_HEADER *rtp, uint16_t len);
  int set_sdp(const StreamId_Ext& stream_id, const char* sdp, int32_t len);
  std::string get_sdp(const StreamId_Ext& stream_id);
  avformat::RTP_FIXED_HEADER* get_rtp_by_seq(const StreamId_Ext& stream_id, bool video, uint16_t seq, uint16_t& len);

  // timer: check m_caches timeout

protected:
  RtpCacheManager();
  static RtpCacheManager *m_inst;
  std::map<uint32_t, media_manager::RTPMediaCache*> m_caches;
  //std::set<RtpCacheWatcher*> m_watches;
};
