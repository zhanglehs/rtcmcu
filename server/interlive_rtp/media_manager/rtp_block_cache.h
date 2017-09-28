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

namespace media_manager {
  class MediaManagerRTPInterface;

  class RTPCircularCache {
  public:
    RTPCircularCache(StreamId_Ext& stream_id);
    ~RTPCircularCache();

    int32_t set_manager(MediaManagerRTPInterface*);

    void init(uint32_t sample_rate, uint32_t max_size);
    bool is_inited();
    void reset();

    int32_t set_rtp(const avformat::RTP_FIXED_HEADER*, uint16_t len, int32_t& status);

    avformat::RTP_FIXED_HEADER* get_by_seq(uint16_t seq, uint16_t& len, int32_t& status_code);

    uint16_t size();

    uint32_t get_ssrc();

    //uint64_t get_last_push_relative_timestamp_ms();

  protected:
    void clean();

    //void _set_push_active();

    uint32_t _adjust();

    int _fill_in(const avformat::RTP_FIXED_HEADER*, uint16_t);

    void _push_back(const avformat::RTP_FIXED_HEADER*, uint16_t);

    StreamId_Ext _stream_id;
    MediaManagerRTPInterface* _media_manager;

    uint32_t _sample_rate;

    uint32_t _ssrc;
    avformat::RTPAVType _av_type;

    uint16_t _max_size;

    //uint32_t _last_push_tick_timestamp;
    //uint64_t _last_push_relative_timestamp_ms;

    std::deque<fragment::RTPBlock> _circular_cache;
  };

  class RTPMediaCache {
  public:
    RTPMediaCache(StreamId_Ext& stream_id);
    virtual ~RTPMediaCache();

    virtual int32_t set_manager(MediaManagerRTPInterface*);

    virtual int32_t set_sdp(const char* sdp, int32_t len);

    virtual avformat::SdpInfo* get_sdp();

    virtual int32_t set_rtp(const avformat::RTP_FIXED_HEADER* rtp, uint16_t len, int32_t& status);

    virtual RTPCircularCache* get_cache_by_ssrc(uint32_t ssrc);

    virtual RTPCircularCache* get_audio_cache();
    virtual RTPCircularCache* get_video_cache();

    virtual time_t get_push_active_time();
    virtual time_t set_push_active_time();

    //virtual uint64_t get_last_push_relative_timestamp_ms();

    virtual bool rtp_empty();

  public:
    StreamId_Ext _stream_id;
    avformat::SdpInfo* _sdp;

    RTPCircularCache* _audio_cache;
    RTPCircularCache* _video_cache;

    time_t _push_active;

    MediaManagerRTPInterface* _media_manager;

    //uint64_t _last_push_relative_timestamp_ms;
  };
}
