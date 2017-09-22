#include "h264.h"
#include <stdio.h>
#include <string.h>
#ifdef WIN32
#include <Windows.h>
#else
#include <netinet/in.h>
#endif
namespace avcodec
{
  static uint8_t payload[2 * 1024 * 1024];
  BitVector::BitVector(unsigned char* baseBytePtr,
    unsigned baseBitOffset,
    unsigned totNumBits) {
    setup(baseBytePtr, baseBitOffset, totNumBits);
  }

  void BitVector::setup(unsigned char* baseBytePtr,
    unsigned baseBitOffset,
    unsigned totNumBits) {
    fBaseBytePtr = baseBytePtr;
    fBaseBitOffset = baseBitOffset;
    fTotNumBits = totNumBits;
    fCurBitIndex = 0;
  }

  static unsigned char const singleBitMask[8]
    = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

#define MAX_LENGTH 32

  void BitVector::putBits(unsigned from, unsigned numBits) {
    if (numBits == 0) return;

    unsigned char tmpBuf[4];
    unsigned overflowingBits = 0;

    if (numBits > MAX_LENGTH) {
      numBits = MAX_LENGTH;
    }

    if (numBits > fTotNumBits - fCurBitIndex) {
      overflowingBits = numBits - (fTotNumBits - fCurBitIndex);
    }

    tmpBuf[0] = (unsigned char)(from >> 24);
    tmpBuf[1] = (unsigned char)(from >> 16);
    tmpBuf[2] = (unsigned char)(from >> 8);
    tmpBuf[3] = (unsigned char)from;

    shiftBits(fBaseBytePtr, fBaseBitOffset + fCurBitIndex, /* to */
      tmpBuf, MAX_LENGTH - numBits, /* from */
      numBits - overflowingBits /* num bits */);
    fCurBitIndex += numBits - overflowingBits;
  }

  void BitVector::put1Bit(unsigned bit) {
    // The following is equivalent to "putBits(..., 1)", except faster:
    if (fCurBitIndex >= fTotNumBits) { /* overflow */
      return;
    }
    else {
      unsigned totBitOffset = fBaseBitOffset + fCurBitIndex++;
      unsigned char mask = singleBitMask[totBitOffset % 8];
      if (bit) {
        fBaseBytePtr[totBitOffset / 8] |= mask;
      }
      else {
        fBaseBytePtr[totBitOffset / 8] &= ~mask;
      }
    }
  }

  unsigned BitVector::getBits(unsigned numBits) {
    if (numBits == 0) return 0;

    unsigned char tmpBuf[4];
    unsigned overflowingBits = 0;

    if (numBits > MAX_LENGTH) {
      numBits = MAX_LENGTH;
    }

    if (numBits > fTotNumBits - fCurBitIndex) {
      overflowingBits = numBits - (fTotNumBits - fCurBitIndex);
    }

    shiftBits(tmpBuf, 0, /* to */
      fBaseBytePtr, fBaseBitOffset + fCurBitIndex, /* from */
      numBits - overflowingBits /* num bits */);
    fCurBitIndex += numBits - overflowingBits;

    unsigned result
      = (tmpBuf[0] << 24) | (tmpBuf[1] << 16) | (tmpBuf[2] << 8) | tmpBuf[3];
    result >>= (MAX_LENGTH - numBits); // move into low-order part of word
    result &= (0xFFFFFFFF << overflowingBits); // so any overflow bits are 0
    return result;
  }

  unsigned BitVector::get1Bit() {
    // The following is equivalent to "getBits(1)", except faster:

    if (fCurBitIndex >= fTotNumBits) { /* overflow */
      return 0;
    }
    else {
      unsigned totBitOffset = fBaseBitOffset + fCurBitIndex++;
      unsigned char curFromByte = fBaseBytePtr[totBitOffset / 8];
      unsigned result = (curFromByte >> (7 - (totBitOffset % 8))) & 0x01;
      return result;
    }
  }

  void BitVector::skipBits(unsigned numBits) {
    if (numBits > fTotNumBits - fCurBitIndex) { /* overflow */
      fCurBitIndex = fTotNumBits;
    }
    else {
      fCurBitIndex += numBits;
    }
  }

  unsigned BitVector::get_expGolomb() {
    unsigned numLeadingZeroBits = 0;
    unsigned codeStart = 1;

    while (get1Bit() == 0 && fCurBitIndex < fTotNumBits) {
      ++numLeadingZeroBits;
      codeStart *= 2;
    }

    return codeStart - 1 + getBits(numLeadingZeroBits);
  }

  void shiftBits(unsigned char* toBasePtr, unsigned toBitOffset,
    unsigned char const* fromBasePtr, unsigned fromBitOffset,
    unsigned numBits) {
    if (numBits == 0) return;

    /* Note that from and to may overlap, if from>to */
    unsigned char const* fromBytePtr = fromBasePtr + fromBitOffset / 8;
    unsigned fromBitRem = fromBitOffset % 8;
    unsigned char* toBytePtr = toBasePtr + toBitOffset / 8;
    unsigned toBitRem = toBitOffset % 8;

    while (numBits-- > 0) {
      unsigned char fromBitMask = singleBitMask[fromBitRem];
      unsigned char fromBit = (*fromBytePtr)&fromBitMask;
      unsigned char toBitMask = singleBitMask[toBitRem];

      if (fromBit != 0) {
        *toBytePtr |= toBitMask;
      }
      else {
        *toBytePtr &= ~toBitMask;
      }

      if (++fromBitRem == 8) {
        ++fromBytePtr;
        fromBitRem = 0;
      }
      if (++toBitRem == 8) {
        ++toBytePtr;
        toBitRem = 0;
      }
    }
  }

  CH264ConfigParser::CH264ConfigParser() :
    m_width(-1), m_height(-1),
    num_units_in_tick(30), time_scale(1), fixed_frame_rate_flag(0){

  }

  CH264ConfigParser::~CH264ConfigParser(){

  }

  bool CH264ConfigParser::ParseSPS(const unsigned char *buffer, int buflen)
  {
    //num_units_in_tick = time_scale = fixed_frame_rate_flag = 0; // default values

    // Begin by making a copy of the NAL unit data, removing any 'emulation prevention' bytes:
    uint8_t sps[1000];
    unsigned spsSize = buflen;
    memcpy(sps, buffer, buflen);
    //removeEmulationBytes(sps, sizeof sps, spsSize);
    //uint8_t* sps = strdup();
    BitVector bv(sps, 0, 8 * spsSize);

    bv.skipBits(8); // forbidden_zero_bit; nal_ref_idc; nal_unit_type
    unsigned profile_idc = bv.getBits(8);
    bv.getBits(8); // constraint_setN_flag // also "reserved_zero_2bits" at end
    bv.getBits(8); // level_idc
    bv.get_expGolomb(); // seq_parameter_set_id
    if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 244 || profile_idc == 44 || profile_idc == 83 || profile_idc == 86 || profile_idc == 118 || profile_idc == 128) {
      unsigned chroma_format_idc = bv.get_expGolomb();
      if (chroma_format_idc == 3) {
        separate_colour_plane_flag = bv.get1BitBoolean();
      }
      (void)bv.get_expGolomb(); // bit_depth_luma_minus8
      (void)bv.get_expGolomb(); // bit_depth_chroma_minus8
      bv.skipBits(1); // qpprime_y_zero_transform_bypass_flag
      unsigned seq_scaling_matrix_present_flag = bv.get1Bit();
      if (seq_scaling_matrix_present_flag) {
        for (int i = 0; i < ((chroma_format_idc != 3) ? 8 : 12); ++i) {
          unsigned seq_scaling_list_present_flag = bv.get1Bit();
          if (seq_scaling_list_present_flag) {
            unsigned sizeOfScalingList = i < 6 ? 16 : 64;
            unsigned lastScale = 8;
            unsigned nextScale = 8;
            for (unsigned j = 0; j < sizeOfScalingList; ++j) {
              if (nextScale != 0) {
                unsigned delta_scale = bv.get_expGolomb();
                nextScale = (lastScale + delta_scale + 256) % 256;
              }
              lastScale = (nextScale == 0) ? lastScale : nextScale;
            }
          }
        }
      }
    }
    unsigned log2_max_frame_num_minus4 = bv.get_expGolomb();
    log2_max_frame_num = log2_max_frame_num_minus4 + 4;
    unsigned pic_order_cnt_type = bv.get_expGolomb();
    if (pic_order_cnt_type == 0) {
      bv.get_expGolomb(); // log2_max_pic_order_cnt_lsb_minus4
    }
    else if (pic_order_cnt_type == 1) {
      bv.skipBits(1); // delta_pic_order_always_zero_flag
      (void)bv.get_expGolomb(); // offset_for_non_ref_pic
      (void)bv.get_expGolomb(); // offset_for_top_to_bottom_field
      unsigned num_ref_frames_in_pic_order_cnt_cycle = bv.get_expGolomb();
      for (unsigned i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i) {
        (void)bv.get_expGolomb(); // offset_for_ref_frame[i]
      }
    }
    bv.get_expGolomb(); // max_num_ref_frames
    bv.get1Bit(); // gaps_in_frame_num_value_allowed_flag
    //==============================width=======================================
    unsigned pic_width_in_mbs_minus1 = bv.get_expGolomb();
    m_width = (pic_width_in_mbs_minus1 + 1) * 16;
    //==============================width=======================================
    //=============================height=======================================
    unsigned pic_height_in_map_units_minus1 = bv.get_expGolomb();
    m_height = (pic_height_in_map_units_minus1 + 1) * 16;
    //=============================height=======================================
    frame_mbs_only_flag = bv.get1BitBoolean();
    if (!frame_mbs_only_flag) {
      bv.skipBits(1); // mb_adaptive_frame_field_flag
    }
    bv.skipBits(1); // direct_8x8_inference_flag
    unsigned frame_cropping_flag = bv.get1Bit();
    if (frame_cropping_flag) {
      (void)bv.get_expGolomb(); // frame_crop_left_offset
      (void)bv.get_expGolomb(); // frame_crop_right_offset
      (void)bv.get_expGolomb(); // frame_crop_top_offset
      (void)bv.get_expGolomb(); // frame_crop_bottom_offset
    }
    unsigned vui_parameters_present_flag = bv.get1Bit();
    if (vui_parameters_present_flag) {
      analyze_vui_parameters(bv);
    }

    return 0;
  }

  void CH264ConfigParser::analyze_vui_parameters(BitVector& bv) {
    unsigned aspect_ratio_info_present_flag = bv.get1Bit();
    if (aspect_ratio_info_present_flag) {
      unsigned aspect_ratio_idc = bv.getBits(8);
      if (aspect_ratio_idc == 255/*Extended_SAR*/) {
        bv.skipBits(32); // sar_width; sar_height
      }
    }
    unsigned overscan_info_present_flag = bv.get1Bit();
    if (overscan_info_present_flag) {
      bv.skipBits(1); // overscan_appropriate_flag
    }
    unsigned video_signal_type_present_flag = bv.get1Bit();
    if (video_signal_type_present_flag) {
      bv.skipBits(4); // video_format; video_full_range_flag
      unsigned colour_description_present_flag = bv.get1Bit();
      if (colour_description_present_flag) {
        bv.skipBits(24); // colour_primaries; transfer_characteristics; matrix_coefficients
      }
    }
    unsigned chroma_loc_info_present_flag = bv.get1Bit();
    if (chroma_loc_info_present_flag) {
      (void)bv.get_expGolomb(); // chroma_sample_loc_type_top_field
      (void)bv.get_expGolomb(); // chroma_sample_loc_type_bottom_field
    }
    unsigned timing_info_present_flag = bv.get1Bit();
    if (timing_info_present_flag) {
      num_units_in_tick = bv.getBits(32);
      time_scale = bv.getBits(32);
      fixed_frame_rate_flag = bv.get1Bit();
    }

  }

  int CH264ConfigParser::AVCEncoderConfigurationRecord(uint8_t* buffer, int maxLen,
    uint8_t* sps, int sps_size, uint8_t* pps, int pps_size){

    uint8_t* p = buffer;
    *p++ = 1;           /* version */
    *p++ = sps[1];      /* profile */
    *p++ = sps[2];      /* profile compat */
    *p++ = sps[3];      /* level */
    *p++ = 0xff;        /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
    *p++ = 0xe1;        /* 3 bits reserved (111) + 5 bits number of sps (00001) */

    *p++ = sps_size >> 8;
    *p++ = sps_size & 0xff;

    for (int i = 0; i < sps_size; i++){
      *p++ = sps[i];
    }
    *p++ = 1;            /* number of pps */

    *p++ = pps_size >> 8;
    *p++ = pps_size & 0xff;

    for (int i = 0; i < pps_size; i++){
      *p++ = pps[i];
    }

    return p - buffer;
  }

  int CH264ConfigParser::AVCDecoderConfigurationRecord(uint8_t* buffer, int maxLen,
    uint8_t* sps, int& sps_size, uint8_t* pps, int& pps_size){
    if (!buffer || !sps || !pps)
    {
      return -1;
    }

    int offset = 6;
    sps_size = ntohs(*(short *)(buffer + offset));
    offset += 2;
    memcpy(sps, buffer + offset, sps_size);
    offset += sps_size;
    offset++;
    pps_size = ntohs(*(short *)(buffer + offset));
    offset += 2;
    memcpy(pps, buffer + offset, pps_size);
    return 0;
  }


  uint8_t* create_h264_payload(x264_nal_t* x264_nal_list, uint32_t nal_num, uint32_t& payload_len)
  {
    if (x264_nal_list == NULL)
    {
      payload_len = 0;
      return NULL;
    }

    uint32_t offset = 0;
    for (unsigned int i = 0; i < nal_num; i++)
    {
      x264_nal_t& x264_nal = x264_nal_list[i];
      offset += x264_nal.i_payload;
    }

    payload_len = offset;
    offset = 0;

    //uint8_t* payload = new uint8_t[payload_len];

    for (unsigned int i = 0; i < nal_num; i++)
    {
      x264_nal_t& x264_nal = x264_nal_list[i];
      memcpy(payload + offset, x264_nal.p_payload, x264_nal.i_payload);
      offset += x264_nal.i_payload;
    }

    return payload;
  }

  // port from ffmpeg avc.c
  // I did not know the details, I hope it is right.
  static const uint8_t *ff_avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
  {
    const uint8_t *a = p + 4 - ((intptr_t)p & 3);

    for (end -= 3; p < a && p < end; p++) {
      if (p[0] == 0 && p[1] == 0 && p[2] == 1)
        return p;
    }

    for (end -= 3; p < end; p += 4) {
      uint32_t x = *(const uint32_t*)p;
      //      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
      //      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
      if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
        if (p[1] == 0) {
          if (p[0] == 0 && p[2] == 1)
            return p;
          if (p[2] == 0 && p[3] == 1)
            return p + 1;
        }
        if (p[3] == 0) {
          if (p[2] == 0 && p[4] == 1)
            return p + 2;
          if (p[4] == 0 && p[5] == 1)
            return p + 3;
        }
      }
    }

    for (end += 3; p < end; p++) {
      if (p[0] == 0 && p[1] == 0 && p[2] == 1)
        return p;
    }

    return end + 3;
  }

  const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end)
  {
    const uint8_t *out = ff_avc_find_startcode_internal(p, end);
    if (p < out && out < end && !out[-1]) out--;
    return out;
  }
}
