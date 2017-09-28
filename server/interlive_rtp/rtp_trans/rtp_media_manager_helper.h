#pragma once

#include "avformat/rtp.h"
#include "streamid.h"
#include <string>

namespace media_manager {
  class MediaManagerRTPInterface;
}

class RTPMediaManagerHelper {
public:
  RTPMediaManagerHelper();
  ~RTPMediaManagerHelper();

public:
  virtual int32_t set_rtp(const StreamId_Ext& stream_id, const avformat::RTP_FIXED_HEADER *rtp, uint16_t len, int32_t& status_code);
  virtual const avformat::RTP_FIXED_HEADER* get_rtp_by_ssrc_seq(const StreamId_Ext& stream_id, uint32_t ssrc,
    uint16_t seq, uint16_t &len, int32_t& status_code);

  virtual int32_t set_sdp_char(const StreamId_Ext& stream_id, const char* sdp, int32_t len, int32_t& status_code);
  virtual std::string get_sdp_str(const StreamId_Ext& stream_id, int32_t& status_code);

protected:
  media_manager::MediaManagerRTPInterface* _media_manager;
};
