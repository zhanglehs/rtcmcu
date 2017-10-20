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


#include "rtp_block_cache.h"
#include "cache_watcher.h"
#include "util/log.h"
#include "time.h"
#include "media_manager_state.h"
#include <assert.h>
#include <algorithm>
#include "cache_manager.h"
#include "avformat/sdp.h"
#include <deque>

namespace media_manager {

  class RTPCircularCache {
  public:
    RTPCircularCache(const StreamId_Ext& stream_id);
    ~RTPCircularCache();

    void init(uint32_t sample_rate, uint32_t max_size);
    bool is_inited();
    void reset();
    uint16_t size();
    uint32_t get_ssrc();

    int32_t set_rtp(const avformat::RTP_FIXED_HEADER*, uint16_t len, int32_t& status);
    avformat::RTP_FIXED_HEADER* get_by_seq(uint16_t seq, uint16_t& len, int32_t& status_code);

  protected:
    void clean();
    uint32_t _adjust();
    int _fill_in(const avformat::RTP_FIXED_HEADER*, uint16_t);

    StreamId_Ext _stream_id;
    uint32_t _sample_rate;
    uint16_t _max_size;
    uint32_t _ssrc;
    avformat::RTPAVType _av_type;
    std::deque<fragment::RTPBlock> _circular_cache;
  };

  class RTPMediaCache {
  public:
    RTPMediaCache(const StreamId_Ext& stream_id);
    ~RTPMediaCache();

    int32_t set_sdp(const char* sdp, int32_t len);
    avformat::SdpInfo* get_sdp();
    int32_t set_rtp(const avformat::RTP_FIXED_HEADER* rtp, uint16_t len, int32_t& status);
    RTPCircularCache* get_audio_cache();
    RTPCircularCache* get_video_cache();

    time_t get_push_active_time();

  protected:
    StreamId_Ext _stream_id;
    avformat::SdpInfo* _sdp;
    RTPCircularCache* _audio_cache;
    RTPCircularCache* _video_cache;

    time_t _push_active;
  };
}

using namespace fragment;
using namespace avformat;
using namespace std;

namespace {
  // if left > right
  bool seq_gt(uint16_t left, uint16_t right) {
    if ((left != right) && ((uint16_t)(left - right) < 0x8000)) {
      return true;
    }
    return false;
  }

  // if left < right
  bool seq_lt(uint16_t left, uint16_t right) {
    if ((left != right) && ((uint16_t)(right - left) < 0x8000)) {
      return true;
    }
    return false;
  }
}

namespace media_manager {

  RTPCircularCache::RTPCircularCache(const StreamId_Ext& stream_id) {
    _stream_id = stream_id;
    reset();
  }

  RTPCircularCache::~RTPCircularCache() {
    clean();
  }

  void RTPCircularCache::init(uint32_t sample_rate, uint32_t max_size) {
    _max_size = max_size;
    _sample_rate = sample_rate;
  }

  bool RTPCircularCache::is_inited() {
    return _sample_rate > 0 && _max_size > 0;
  }

  void RTPCircularCache::reset() {
    _ssrc = 0;
    _sample_rate = 0;
    _max_size = 0;
    _av_type = RTP_AV_NULL;
    clean();
  }

  int32_t RTPCircularCache::set_rtp(const RTP_FIXED_HEADER* rtp, uint16_t len, int32_t& status) {
    if (_sample_rate == 0) {
      return 1;
    }

    if (rtp == NULL || len < sizeof(RTP_FIXED_HEADER)) {
      WRN("rtp invalid, stream_id: %s", _stream_id.unparse().c_str());
      status = RTP_CACHE_RTP_INVALID;
      return -1;
    }

    if (_ssrc == 0) {
      _ssrc = rtp->get_ssrc();
      INF("update ssrc %u payload %u ver %u", rtp->get_ssrc(), rtp->payload, rtp->version);
    }

    if (rtp->get_ssrc() != _ssrc) {
      WRN("rtp ssrc not equal, stream_id: %s, _ssrc: %u, new_ssrc: %u",
        _stream_id.unparse().c_str(), _ssrc, rtp->get_ssrc());
      status = RTP_CACHE_RTP_INVALID;
      return -1;
    }

    uint16_t seq = rtp->get_seq();

    if (_circular_cache.size() == 0)  {
      _circular_cache.push_back(RTPBlock(rtp, len));
      status = STATUS_SUCCESS;
      return 0;
    }

    uint16_t latest_len = 0;
    avformat::RTP_FIXED_HEADER* latest
      = _circular_cache.back().get(latest_len);
    if (latest && _sample_rate > 0
      && abs(int(latest->get_rtp_timestamp() - rtp->get_rtp_timestamp())) >= int(20 * _sample_rate)) {
      clean();
      _circular_cache.push_back(RTPBlock(rtp, len));

      status = STATUS_SUCCESS;
      return 0;
    }

    // this is a circular cache
    // expect_next_max = back.seq + _max_size
    //                         3                      1                    2
    // ||---------|-------------------------|-------------------|------------------||
    //          front.seq                back.seq         expect_next_max
    //            |       cache             |     large seq     |      small seq
    //
    // 1. if back.seq        <  new_block.seq < expect_next_max,  push back and fill empty block
    // 2. if expect_next_max <= new_block.seq < front.seq,         reset cache,
    // 3. if front.seq       <= new_block.seq <= back.seq,         fill in.
    //

    uint16_t back_seq = _circular_cache.back().get_seq();
    uint16_t front_seq = _circular_cache.front().get_seq();
    uint16_t expect_next_max = back_seq + _max_size;

    // 1. if back.seq        < new_block.seq <= expect_next_max,  push back and fill empty block
    if (seq != back_seq && (uint16_t)(seq - back_seq) < _max_size) {
      for (uint16_t idx = back_seq + 1; idx != seq; idx++) {
        RTPBlock block(NULL, 0);
        _circular_cache.push_back(block);
      }
      _circular_cache.push_back(RTPBlock(rtp, len));
      _adjust();

      status = STATUS_SUCCESS;
      return 0;
    }

    // 2. if expect_next_max < new_block.seq < front.seq,         reset cache,
    if ((uint16_t)(seq - expect_next_max) < (uint16_t)(front_seq - expect_next_max)) {
      clean();
      _circular_cache.push_back(RTPBlock(rtp, len));

      status = STATUS_SUCCESS;
      return 0;
    }

    // 3. if front.seq       <= new_block.seq <= back.seq,         fill in.
    if ((uint16_t)(seq - front_seq) <= (uint16_t)(back_seq - front_seq)) {
      _fill_in(rtp, len);
      status = STATUS_SUCCESS;
      return 0;
    }

    ERR("unknown error, stream_id: %s, rtp seq:%u, rtp len: %u, cache_size: %d",
      _stream_id.unparse().c_str(), rtp->get_seq(), len, (int)_circular_cache.size());
    status = RTP_CACHE_UNKNOWN_ERROR;
    assert(0);
    return -1;
  }

  int RTPCircularCache::_fill_in(const RTP_FIXED_HEADER* rtp, uint16_t len) {
    uint16_t seq = rtp->get_seq();
    uint16_t front_seq = _circular_cache.front().get_seq();
    uint32_t idx = (uint16_t)(seq - front_seq);

    if (idx >= _circular_cache.size()) {
      ERR("unknown error, stream_id: %s, rtp seq:%u, front_seq: %u, cache_size: %d",
        _stream_id.unparse().c_str(), rtp->get_seq(), front_seq, (int)_circular_cache.size());
      clean();
      assert(0);
      return -1;
    }

    RTPBlock& block = _circular_cache[idx];
    if (!block.is_valid()) {
      block.set(rtp, len);
    }
    else {
      TRC("this block is already set. stream_id: %s, seq: %u", _stream_id.unparse().c_str(), seq);
    }

    return 0;
  }

  uint16_t RTPCircularCache::size() {
    return _circular_cache.size();
  }

  uint32_t RTPCircularCache::get_ssrc() {
    return _ssrc;
  }

  uint32_t RTPCircularCache::_adjust() {
    while (_circular_cache.size() > _max_size) {
      _circular_cache.front().finalize();
      _circular_cache.pop_front();
    }

    while (_circular_cache.size() > 0 && !_circular_cache.front().is_valid()) {
      _circular_cache.front().finalize();
      _circular_cache.pop_front();
    }

    return _circular_cache.size();
  }

  void RTPCircularCache::clean() {
    while (!_circular_cache.empty()) {
      _circular_cache.front().finalize();
      _circular_cache.pop_front();
    }
  }

  RTP_FIXED_HEADER* RTPCircularCache::get_by_seq(uint16_t seq, uint16_t& len, int32_t& status_code) {
    len = 0;

    if (_circular_cache.size() == 0) {
      INF("cache empty");
      status_code = RTP_CACHE_CIRCULAR_CACHE_EMPTY;
      len = 0;
      return NULL;
    }

    uint16_t back_seq = _circular_cache.back().get_seq();
    uint16_t front_seq = _circular_cache.front().get_seq();

    if (seq_gt(seq, back_seq)) {
      status_code = RTP_CACHE_SEQ_TOO_LARGE;
      return NULL;
    }

    if (seq_lt(seq, front_seq)) {
      status_code = RTP_CACHE_SEQ_TOO_SMALL;
      return NULL;
    }

    uint16_t idx = seq - front_seq;
    RTPBlock& block = _circular_cache[idx];

    if (block.is_valid()) {
      if (seq == block.get_seq()) {
        status_code = STATUS_SUCCESS;
        return block.get(len);
      }
      else {
        ERR("unknown error, stream_id: %s, rtp seq:%u, front_seq: %u, cache_size: %d",
          _stream_id.unparse().c_str(), seq, front_seq, (int)_circular_cache.size());
        len = 0;
        status_code = RTP_CACHE_UNKNOWN_ERROR;
        assert(0);
        return NULL;
      }
    }

    status_code = RTP_CACHE_NO_THIS_SEQ;
    len = 0;
    return NULL;
  }

  RTPMediaCache::RTPMediaCache(const StreamId_Ext& stream_id)
    :_stream_id(stream_id),
    _sdp(NULL),
    _push_active(0) {
    _video_cache = new RTPCircularCache(stream_id);
    _audio_cache = new RTPCircularCache(stream_id);
  }

  RTPMediaCache::~RTPMediaCache() {
    if (_sdp != NULL) {
      delete _sdp;
      _sdp = NULL;
    }

    delete _audio_cache;
    delete _video_cache;
  }

  int32_t RTPMediaCache::set_sdp(const char* sdp, int32_t len) {
    INF("set sdp, sid=%s", _stream_id.unparse().c_str());

    if (_sdp != NULL) {
      delete _sdp;
      _sdp = NULL;
    }
    _sdp = new SdpInfo();
    _sdp->load(sdp, len);

    _audio_cache->reset();
    _video_cache->reset();

    const vector<rtp_media_info*>& _media_infos = _sdp->get_media_infos();
    for (unsigned int i = 0; i < _media_infos.size(); i++) {
      rtp_media_info* media_info = _media_infos[i];
      switch (media_info->payload_type) {
      case RTP_AV_H264:
        _video_cache->init(media_info->rate, 3000);
        break;
      case RTP_AV_AAC:
      case RTP_AV_MP3:
        _audio_cache->init(media_info->rate, 1000);
        break;
      default:
        break;
      }
    }

    _push_active = time(NULL);
    return 0;
  }

  SdpInfo* RTPMediaCache::get_sdp() {
    return _sdp;
  }

  RTPCircularCache* RTPMediaCache::get_audio_cache() {
    return _audio_cache->is_inited() ? _audio_cache : NULL;
  }

  RTPCircularCache* RTPMediaCache::get_video_cache() {
    return _video_cache->is_inited() ? _video_cache : NULL;
  }

  int32_t RTPMediaCache::set_rtp(const RTP_FIXED_HEADER* rtp, uint16_t len, int32_t& status) {
    if (rtp == NULL || len < sizeof(RTP_FIXED_HEADER)) {
      status = RTP_CACHE_RTP_INVALID;
      return status;
    }

    RTPAVType type = (RTPAVType)rtp->payload;

    switch (type) {
    case RTP_AV_MP3:
    case RTP_AV_AAC:
      _audio_cache->set_rtp(rtp, len, status);
      break;
    case RTP_AV_H264:
      _video_cache->set_rtp(rtp, len, status);
      break;
    default:
      status = RTP_CACHE_SET_RTP_FAILED;
      ERR("set rtp failed , invailed payload type %u", type);
      return status;
    }

    if (status == STATUS_SUCCESS) {
      _push_active = time(NULL);
    }
    return status;
  }

  time_t RTPMediaCache::get_push_active_time() {
    return _push_active;
  }

}

//////////////////////////////////////////////////////////////////////////
#include "media_manager/rtp2flv_remuxer.h"
#include "relay/module_backend.h"

RtpCacheManager::RtpCacheManager() {
  m_ev_timer = NULL;
}

RtpCacheManager::~RtpCacheManager() {
  if (m_ev_timer) {
    event_free(m_ev_timer);
    m_ev_timer = NULL;
  }
}

void RtpCacheManager::Init(struct event_base *ev_base) {
  m_ev_timer = event_new(ev_base, -1, EV_PERSIST, OnTimer, this);
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 10000;
  event_add(m_ev_timer, &tv);
}

void RtpCacheManager::AddWatcher(RtpCacheWatcher *watcher){
  m_watches.insert(watcher);
}

void RtpCacheManager::RemoveWatcher(RtpCacheWatcher *watcher) {
  m_watches.erase(watcher);
}

int RtpCacheManager::set_rtp(const StreamId_Ext& stream_id, const avformat::RTP_FIXED_HEADER *rtp, uint16_t len) {
  auto it = m_caches.find(stream_id.get_32bit_stream_id());
  if (it == m_caches.end()) {
    return -1;
  }

  // TODO: zhangle
  if (true) {
    int32_t status_code = 0;
    media_manager::RTP2FLVRemuxer::get_instance()->set_rtp(stream_id, rtp, len, status_code);
  }

  media_manager::RTPMediaCache *cache = it->second;
  int32_t status_code = 0;
  int ret = cache->set_rtp(rtp, len, status_code);
  //if (ret >= 0) {
  //  for (auto it = m_watches.begin(); it != m_watches.end(); it++) {
  //    (*it)->OnRtp();
  //  }
  //}
  return ret;
}

int RtpCacheManager::set_sdp(const StreamId_Ext& stream_id, const char* sdp, int32_t len) {
  media_manager::RTPMediaCache *cache = NULL;
  auto it = m_caches.find(stream_id.get_32bit_stream_id());
  if (it == m_caches.end()) {
    cache = new media_manager::RTPMediaCache(stream_id);
    m_caches[stream_id.get_32bit_stream_id()] = cache;
  }
  else {
    cache = it->second;
  }

  // TODO: zhangle
  if (true) {
    int32_t status_code = 0;
    media_manager::RTP2FLVRemuxer::get_instance()->set_sdp_char(stream_id, sdp, len, status_code);
  }

  int ret = cache->set_sdp(sdp, len);
  if (ret >= 0) {
    for (auto it = m_watches.begin(); it != m_watches.end(); it++) {
      (*it)->OnSdp(stream_id, sdp);
    }
  }
  return ret;
}

std::string RtpCacheManager::get_sdp(const StreamId_Ext& stream_id) {
  auto it = m_caches.find(stream_id.get_32bit_stream_id());
  if (it == m_caches.end()) {
    RelayManager::Instance()->StartPullRtp(stream_id);
    return "";
  }

  media_manager::RTPMediaCache *cache = it->second;
  SdpInfo* sdp = cache->get_sdp();
  assert(sdp);
  return sdp->get_sdp_str();
}

avformat::RTP_FIXED_HEADER* RtpCacheManager::get_rtp_by_seq(const StreamId_Ext& stream_id, bool video, uint16_t seq, uint16_t& len) {
  len = 0;
  auto it = m_caches.find(stream_id.get_32bit_stream_id());
  if (it == m_caches.end()) {
    return NULL;
  }

  media_manager::RTPMediaCache *cache = it->second;
  media_manager::RTPCircularCache *media = NULL;
  if (video) {
    media = cache->get_video_cache();
  }
  else {
    media = cache->get_audio_cache();
  }
  if (media) {
    int32_t status_code = 0;
    return media->get_by_seq(seq, len, status_code);
  }
  return NULL;
}

void RtpCacheManager::OnTimer(evutil_socket_t fd, short flag, void *arg) {
  RtpCacheManager *pThis = (RtpCacheManager*)arg;
  pThis->OnTimerImpl(fd, flag, arg);
}

// 如果一路流很久没有收到数据，应从cache中删除
void RtpCacheManager::OnTimerImpl(evutil_socket_t fd, short flag, void *arg) {
  time_t now = time(NULL);
  for (auto it = m_caches.begin(); it != m_caches.end();) {
    media_manager::RTPMediaCache *c = it->second;
    if (now - c->get_push_active_time() > 20) {
      it = m_caches.erase(it);
    }
    else {
      it++;
    }
  }
}
