#include "rtp_enc.h"
#include "avcodec/h264.h"
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
#include "assert.h"
using namespace std;
using namespace avcodec;

namespace avformat
{
  vector<RTPData>& RTPEncodeH264::generate_packet(
    const uint8_t* payload, uint32_t payload_len,
    uint32_t timestamp, int32_t& status)
  {
    _rtp_data_vec.clear();
    _next_temp_packet = 0;

    if (!_initialized)
    {
      // ERROR
      status = -1;
      return _rtp_data_vec;
    }

    const uint8_t *end = payload + payload_len;
    const uint8_t *r;

    r = ff_avc_find_startcode(payload, end);

    if (r != end)
    {
      while (r < end)
      {
        const uint8_t *r1;
        while (!*(r++));
        r1 = ff_avc_find_startcode(r, end);

        nal_generate(r, timestamp, r1 - r, r1 == end);

        r = r1;
      }
    }
    else {
      r = payload;
      //skip avc0
      if ((*(r + 4) != 0xff) || (*(r + 5) != 0xe1))
      {
        while (r < end)
        {
          int nalu_size = ntohl(*((int *)r));
          uint8_t nalu_type = *(r + 4);
          r += 4;
          if (nalu_type != avcodec::NAL_SEI
            && nalu_type != avcodec::NAL_SPS
            && nalu_type != avcodec::NAL_PPS)
          {
            nal_generate(r, timestamp, nalu_size, r + nalu_size == end);
          }
          r += nalu_size;
        }
      }
    }

    return _rtp_data_vec;
  }

  RTPEncodeH264::~RTPEncodeH264()
  {

  }

  void RTPEncodeH264::nal_generate(const uint8_t *buf, uint32_t timestamp, int size, int last)
  {
    NALU_t nalu;

    nalu.buf = (uint8_t*)buf;
    nalu.forbidden_bit = buf[0] & 0x80; //1 bit  
    nalu.nal_reference_idc = buf[0] & 0x60; // 2 bit  
    nalu.nal_unit_type = (buf[0]) & 0x1f;// 5 bit
    nalu.len = size;


    if (size < get_max_payload_len())
    {
      uint32_t write_offset = sizeof(RTP_FIXED_HEADER);

      RTP_FIXED_HEADER *rtp_hdr = get_current_packet();
      rtp_hdr = build_rtp_header(rtp_hdr, timestamp, last);

      NALU_HEADER     *nalu_hdr;
      nalu_hdr = (NALU_HEADER*)((uint8_t *)(rtp_hdr)+write_offset);
      nalu_hdr->F = nalu.forbidden_bit;
      nalu_hdr->NRI = nalu.nal_reference_idc >> 5;
      nalu_hdr->TYPE = nalu.nal_unit_type;
      write_offset++;

      memcpy((uint8_t *)(nalu_hdr)+1, nalu.buf + 1, nalu.len - 1);

      write_offset += nalu.len - 1;

      RTPData data;
      data.header = rtp_hdr;
      data.len = write_offset;

      _rtp_data_vec.push_back(data);
    }
    else
    {
      int packet_num = nalu.len / get_max_payload_len();
      if (nalu.len % get_max_payload_len() != 0)
      {
        packet_num++;
      }

      int lastPackSize = nalu.len - (packet_num - 1)*get_max_payload_len();

      for (int packetIndex = 0; packetIndex < packet_num; packetIndex++)
      {
        uint32_t write_offset = sizeof(RTP_FIXED_HEADER);
        RTP_FIXED_HEADER *rtp_hdr = get_current_packet();

        int last_maker = 1;
        if (packetIndex != (packet_num - 1))
        {
          last_maker = 0;
        }

        rtp_hdr = build_rtp_header(rtp_hdr, timestamp, last_maker && last);

        FU_INDICATOR    *fu_ind;
        FU_HEADER       *fu_hdr;

        fu_ind = (FU_INDICATOR*)((uint8_t *)(rtp_hdr)+write_offset);
        fu_ind->F = nalu.forbidden_bit;
        fu_ind->NRI = nalu.nal_reference_idc >> 5;
        fu_ind->TYPE = 28; // FU Indicator; Type = 28 ---> FU-A 

        write_offset++;

        fu_hdr = (FU_HEADER*)((uint8_t *)(rtp_hdr)+write_offset);
        fu_hdr->S = packetIndex == 0 ? 1 : 0;
        fu_hdr->E = packetIndex == packet_num - 1 ? 1 : 0;
        fu_hdr->R = 0;
        fu_hdr->TYPE = nalu.nal_unit_type;

        write_offset++;

        uint16_t actual_payload_size = get_max_payload_len();

        if (packetIndex == packet_num - 1)
        {
          actual_payload_size = lastPackSize - 1;
        }

        memcpy((uint8_t *)(rtp_hdr)+write_offset, nalu.buf + (packetIndex)*get_max_payload_len() + 1, actual_payload_size);
        write_offset += actual_payload_size;

        RTPData data;
        data.header = rtp_hdr;
        data.len = write_offset;

        _rtp_data_vec.push_back(data);
      }
    }

  }

  RTP_FIXED_HEADER* RTPEncodeH264::get_current_packet()
  {
    if (_next_temp_packet >= _temp_packets.size())
    {
      int rtp_packet_count = _temp_packets.size();
      for (int i = 0; i < rtp_packet_count; i++)
      {
        uint8_t* data = new uint8_t[_max_rtp_packet_len];
        _temp_packets.push_back((RTP_FIXED_HEADER*)data);
      }
    }

    RTP_FIXED_HEADER* header = _temp_packets[_next_temp_packet];
    _next_temp_packet++;
    return header;
  }

  RTP_FIXED_HEADER* RTPEncodeH264::build_rtp_header(RTP_FIXED_HEADER* rtp_header, uint32_t timestamp, int maker)
  {
    memset(rtp_header, 0, sizeof(RTP_FIXED_HEADER));

    rtp_header->version = 2;
    rtp_header->marker = maker;
    rtp_header->ssrc = htonl(_ssrc);
    rtp_header->payload = RTP_AV_H264;
    rtp_header->seq_no = htons(_seq_num++);
    rtp_header->timestamp = htonl(((uint64_t)timestamp)* _sample_rate / 1000);
    rtp_header->csrc_len = 0;
    rtp_header->extension = 0;
    rtp_header->padding = 0;

    return rtp_header;
  }

}
