#include "rtp_media_manager_helper.h"

#include "media_manager/cache_manager.h"

using namespace media_manager;
using namespace avformat;
using namespace fragment;

RTPMediaManagerHelper::RTPMediaManagerHelper() {
  _media_manager = CacheManager::get_rtp_cache_instance();
}

RTPMediaManagerHelper::~RTPMediaManagerHelper() {
}

int RTPMediaManagerHelper::set_rtp(const StreamId_Ext& stream_id, const RTP_FIXED_HEADER *rtp, uint16_t len, int32_t& status_code) {
  RTPMediaCache* media_cache = _media_manager->get_rtp_media_cache(stream_id, status_code, false);
  if (media_cache == NULL) {
    return -1;
  }

  int ret = media_cache->set_rtp(rtp, len, status_code);
  if (ret < 0) {
    return -1;
  }
  return 0;
}

int32_t RTPMediaManagerHelper::set_sdp_char(const StreamId_Ext& stream_id, const char* sdp, int32_t len, int32_t& status_code) {
  RTPMediaCache* media_cache = _media_manager->get_rtp_media_cache(stream_id, status_code, false);
  if (media_cache == NULL) {
    return status_code;
  }

  return media_cache->set_sdp(sdp, len);
}

std::string RTPMediaManagerHelper::get_sdp_str(const StreamId_Ext& stream_id, int32_t& status_code) {
  RTPMediaCache* media_cache = _media_manager->get_rtp_media_cache(stream_id, status_code, true);
  if (media_cache == NULL) {
    status_code = media_manager::RTP_CACHE_NO_THIS_STREAM;
    return "";
  }
  SdpInfo* sdp = media_cache->get_sdp();
  if (sdp == NULL) {
    return std::string("");
  }

  return sdp->get_sdp_str();
}
