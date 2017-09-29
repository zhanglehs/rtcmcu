/**
* @file
* @brief
* @author   songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2015/07/24
* @see
*/


#pragma once

#include <deque>
#include <vector>
#include "fragment/rtp_block.h"
#include "avformat/sdp.h"
#include "streamid.h"
#include "util/port.h"
#include <map>
#include <set>

namespace media_manager {
  class MediaManagerRTPInterface;

  class RTPCircularCache {
  public:
    RTPCircularCache(const StreamId_Ext& stream_id);
    ~RTPCircularCache();

    int32_t set_manager(MediaManagerRTPInterface*);

    void init(uint32_t sample_rate, uint32_t max_size);
    bool is_inited();
    void reset();

    int32_t set_rtp(const avformat::RTP_FIXED_HEADER*, uint16_t len, int32_t& status);

    avformat::RTP_FIXED_HEADER* get_by_seq(uint16_t seq, uint16_t& len, int32_t& status_code);

    uint16_t size();

    uint32_t get_ssrc();

  protected:
    void clean();

    uint32_t _adjust();

    int _fill_in(const avformat::RTP_FIXED_HEADER*, uint16_t);

    StreamId_Ext _stream_id;
    MediaManagerRTPInterface* _media_manager;

    uint32_t _sample_rate;

    uint32_t _ssrc;
    avformat::RTPAVType _av_type;

    uint16_t _max_size;

    std::deque<fragment::RTPBlock> _circular_cache;
  };

  class RTPMediaCache {
  public:
    RTPMediaCache(const StreamId_Ext& stream_id);
    ~RTPMediaCache();

    int32_t set_manager(MediaManagerRTPInterface*);

    int32_t set_sdp(const char* sdp, int32_t len);

    avformat::SdpInfo* get_sdp();

    int32_t set_rtp(const avformat::RTP_FIXED_HEADER* rtp, uint16_t len, int32_t& status);

    RTPCircularCache* get_audio_cache();
    RTPCircularCache* get_video_cache();

    time_t get_push_active_time();

    bool rtp_empty();

  protected:
    StreamId_Ext _stream_id;
    avformat::SdpInfo* _sdp;

    RTPCircularCache* _audio_cache;
    RTPCircularCache* _video_cache;

    time_t _push_active;

    MediaManagerRTPInterface* _media_manager;
  };
}

class RtpCacheWatcher {
public:
  virtual ~RtpCacheWatcher() {}
  virtual void OnRtp() {}
  virtual void OnSdp() {}
};

class RtpCacheManager {
public:
  static RtpCacheManager* Instance();
  static void DestroyInstance();

  int Init(struct event_base *ev_base);
  void AddWatcher(RtpCacheWatcher *watcher);
  void RemoveWatcher(RtpCacheWatcher *watcher);

  int set_rtp(const StreamId_Ext& stream_id, const avformat::RTP_FIXED_HEADER *rtp, uint16_t len);
  int set_sdp(const StreamId_Ext& stream_id, const char* sdp, int32_t len);
  std::string get_sdp(const StreamId_Ext& stream_id);
  avformat::RTP_FIXED_HEADER* get_rtp_by_seq(const StreamId_Ext& stream_id, bool video, uint16_t seq, uint16_t& len);

  // timer: check m_caches timeout

protected:
  RtpCacheManager();
  static RtpCacheManager *m_inst;
  std::map<uint32_t, media_manager::RTPMediaCache*> m_caches;
  std::set<RtpCacheWatcher*> m_watches;
};
