#include "rtp_enc.h"
#ifndef WIN32
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#else
#include <Windows.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "../avcodec/aac.h"

using namespace std;

namespace avformat
{
  vector<RTPData>& RTPEncodeAAC::generate_packet(
    const uint8_t* payload, uint32_t payload_len,
    uint32_t timestamp, int32_t& status)
  {
    _rtp_data_vec.clear();
    RTP_FIXED_HEADER *rtp_hdr = _temp_packets[0];
    rtp_hdr = build_rtp_header(rtp_hdr, timestamp, 1);

    uint32_t w_offset = sizeof(RTP_FIXED_HEADER);

    if (_out_format == AAC_ADTS)
    {
      avcodec::AACConfig config;
      config.set_samplingFrequency(_sample_rate);
      config.set_audioObjectType(AACPLUS_AOT_AAC_LC);
      config.set_frame_length(payload_len + ADTS_HEADER_SIZE);
      config.set_channelConfig(_channel);

      int header_len = 0;
      uint8_t *header_data = config.build_aac_adts(header_len);
      memcpy((uint8_t *)(rtp_hdr)+w_offset, header_data, header_len);
      w_offset += header_len;
    }
    else if (_out_format == AAC_LATM)
    {
      LATM_HEADER* latm_header = (LATM_HEADER*)((uint8_t *)(rtp_hdr)+w_offset);
      latm_header->au_count[0] = 0x00;
      latm_header->au_count[1] = 0x10;
      latm_header->first_au_size[0] = (payload_len & 0x1fe0) >> 5;
      latm_header->first_au_size[1] = (payload_len & 0x1f) << 3;
      w_offset += sizeof(LATM_HEADER);
    }

    memcpy((uint8_t *)(rtp_hdr)+w_offset, payload, payload_len);
    w_offset += payload_len;

    RTPData data;
    data.header = rtp_hdr;
    data.len = w_offset;

    _rtp_data_vec.push_back(data);

    return _rtp_data_vec;
  }

  void RTPEncodeAAC::set_channel(int channel) {
    _channel = channel;
  }


  RTPEncodeAAC::~RTPEncodeAAC()
  {
  }

  RTP_FIXED_HEADER* RTPEncodeAAC::build_rtp_header(RTP_FIXED_HEADER* rtp_header, uint32_t timestamp, int maker)
  {
    memset(rtp_header, 0, sizeof(RTP_FIXED_HEADER));

    rtp_header->version = 2;
    rtp_header->marker = maker;
    rtp_header->ssrc = htonl(_ssrc);
    rtp_header->payload = RTP_AV_AAC;
    rtp_header->seq_no = htons(_seq_num++);
    rtp_header->timestamp = htonl(((uint64_t)timestamp)* _sample_rate / 1000);
    rtp_header->csrc_len = 0;
    rtp_header->extension = 0;
    rtp_header->padding = 0;

    return rtp_header;
  }
}
