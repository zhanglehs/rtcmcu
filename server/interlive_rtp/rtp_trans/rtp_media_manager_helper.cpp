/*
 * Author: hechao@youku.com 
 *
 */
#include "rtp_media_manager_helper.h"
#include "media_manager/cache_manager.h"
#include "avformat/rtp.h"

using namespace media_manager;
using namespace avformat;
using namespace fragment;
using namespace std;

RTPMediaManagerHelper::RTPMediaManagerHelper()
{
    _media_manager = CacheManager::get_rtp_cache_instance();
}

RTPMediaManagerHelper::~RTPMediaManagerHelper()
{
}

int RTPMediaManagerHelper::set_rtp(const StreamId_Ext& stream_id, const RTP_FIXED_HEADER *rtp, uint16_t len, int32_t& status_code)
{

    RTPMediaCache* media_cache = _media_manager->get_rtp_media_cache(stream_id, status_code, false);
    if (media_cache == NULL)
    {
        return -1;
    }

    int ret = media_cache->set_rtp(rtp, len, status_code);

    if (ret < 0)
    {
        // WRN
        return -1;
    }

    return 0;
}


const RTP_FIXED_HEADER* RTPMediaManagerHelper::get_rtp_by_ssrc_seq(const StreamId_Ext& stream_id, uint32_t ssrc, uint16_t seq,
    uint16_t &len, int32_t& status_code, bool return_next_valid_packet)
{
    RTPMediaCache* media_cache = _media_manager->get_rtp_media_cache(stream_id, status_code, true);
    if (media_cache == NULL)
    {
        // WRN
        return NULL;
    }

    RTPCircularCache* circular_cache = media_cache->get_cache_by_ssrc(ssrc);
    if (circular_cache == NULL)
    {
        // WRN
        status_code = media_manager::RTP_CACHE_NO_THIS_TRACK;
        return NULL;
    }

    RTP_FIXED_HEADER* rtp = circular_cache->get_by_seq(seq, len, status_code, return_next_valid_packet);

    return rtp;
}

const RTP_FIXED_HEADER* RTPMediaManagerHelper::get_rtp_by_mediatype_seq(const StreamId_Ext& stream_id, RTPMediaType type, uint16_t seq,
    uint16_t &len, int32_t& status_code, bool return_next_valid_packet)
{
    RTPMediaCache* media_cache = _media_manager->get_rtp_media_cache(stream_id, status_code, true);
    if (media_cache == NULL)
    {
        // WRN
        status_code = media_manager::RTP_CACHE_NO_THIS_STREAM;
        return NULL;
    }

    RTPCircularCache* circular_cache = media_cache->get_cache_by_media(type);
    if (circular_cache == NULL)
    {
        // WRN
        status_code = media_manager::RTP_CACHE_NO_THIS_TRACK;
        return NULL;
    }

    RTP_FIXED_HEADER* rtp = circular_cache->get_by_seq(seq, len, status_code, return_next_valid_packet);

    return rtp;
}

int32_t RTPMediaManagerHelper::set_sdp_char(const StreamId_Ext& stream_id, const char* sdp, int32_t len, int32_t& status_code)
{
    RTPMediaCache* media_cache = _media_manager->get_rtp_media_cache(stream_id, status_code, false);
    if (media_cache == NULL)
    {
        // WRN
        return status_code;
    }

    int32_t ret = media_cache->set_sdp(sdp, len);

    return ret;
}

int32_t RTPMediaManagerHelper::set_sdp_str(const StreamId_Ext& stream_id, const string& sdp, int32_t& status_code)
{
    RTPMediaCache* media_cache = _media_manager->get_rtp_media_cache(stream_id, status_code, false);
    if (media_cache == NULL)
    {
        // WRN
        return status_code;
    }

    int32_t ret = media_cache->set_sdp(sdp.c_str(), sdp.length());

    return ret;
}

const char* RTPMediaManagerHelper::get_sdp_char(const StreamId_Ext& stream_id, int32_t& len, int32_t& status_code)
{
    RTPMediaCache* media_cache = _media_manager->get_rtp_media_cache(stream_id, status_code, true);
    if (media_cache == NULL)
    {
        // WRN
        return NULL;
    }

    SdpInfo* sdp = media_cache->get_sdp();

    if (sdp == NULL)
    {
        return NULL;
    }

    const char* sdp_char = sdp->get_sdp_char();
    if (sdp_char == NULL)
    {
        return NULL;
    }

    len = strlen(sdp_char);
    return sdp_char;
}

string RTPMediaManagerHelper::get_sdp_str(const StreamId_Ext& stream_id, int32_t& status_code)
{
    RTPMediaCache* media_cache = _media_manager->get_rtp_media_cache(stream_id, status_code, true);
    if (media_cache == NULL)
    {
        // WRN
        status_code = media_manager::RTP_CACHE_NO_THIS_STREAM;
        return "";
    }

    SdpInfo* sdp = media_cache->get_sdp();

    if (sdp == NULL)
    {
        return string("");
    }

    return sdp->get_sdp_str();
}

int32_t RTPMediaManagerHelper::set_uploader_ntp(const StreamId_Ext& stream_id,
  avformat::RTPAVType type, uint32_t ntp_secs, uint32_t ntp_frac, uint32_t rtp) {
  int32_t status_code = 0;
  RTPMediaCache* media_cache
    = _media_manager->get_rtp_media_cache(stream_id, status_code, false);
  if (media_cache == NULL) {
    return -1;
  }

  media_cache->set_uploader_ntp(type, ntp_secs, ntp_frac, rtp);
  return 0;
}
