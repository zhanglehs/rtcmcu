#include "rtp_enc.h"

namespace avformat
{
  RTPEncode::RTPEncode()
  {
    _initialized = false;
  }

  int32_t RTPEncode::init(uint32_t ssrc, uint32_t sample_rate,
    uint16_t max_rtp_packet_len,
    uint16_t init_rtp_packet_count)
  {
    _ssrc = ssrc;
    _sample_rate = sample_rate;
    _seq_num = 0;

    if (init_rtp_packet_count == 0)
    {
      // ERR
      return -1;
    }

    if (max_rtp_packet_len <= sizeof(RTP_FIXED_HEADER))
    {
      // ERR
      return -1;
    }

    _max_rtp_packet_len = max_rtp_packet_len;
    _init_rtp_packet_count = init_rtp_packet_count;

    for (int i = 0; i < init_rtp_packet_count; i++)
    {
      uint8_t* data = new uint8_t[_max_rtp_packet_len];
      _temp_packets.push_back((RTP_FIXED_HEADER*)data);
    }

    _initialized = true;

    return 0;
  }

  RTPEncode::~RTPEncode()
  {
    for (uint32_t i = 0; i < _temp_packets.size(); i++)
    {
      delete _temp_packets[i];
    }
  }

  uint16_t RTPEncode::get_max_payload_len()
  {
    return _max_rtp_packet_len - sizeof(RTP_FIXED_HEADER)-10;
  }
}
