/**
* @file fragment_generator.cpp
* @brief	implement of fragment_generator.h \n
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/11/21
* @see  fragment_generator.h
*/

#include "../util/log.h"
#include "../util/flv.h"
#include "../util/flv2ts.h"

#include "cmd_protocol/proto_define.h"
#include "fragment_generator.h"
#include <fstream>
#include <algorithm>
#include <deque>

using namespace std;
using namespace fragment;

#define FRAGMENT_INIT_LEN 512*1024
#define FRAGMENT_MAX_LEN 8*1024*1024

namespace fragment
{
  FragmentGenerator::FragmentGenerator(StreamId_Ext& stream_id)
    : _main_type(0),
    _last_main_tag_timestamp(0),
    _flv_header(NULL),
    _stream_id(stream_id)
  {
    _flv_header = NULL;
    _input_tag_num = 0;
  }

  FragmentGenerator::~FragmentGenerator() {
    if (_flv_header != NULL) {
      free(_flv_header);
      _flv_header = NULL;
    }
  }

  int32_t FragmentGenerator::_key_type(flv_tag* input_flv_tag) {
    if (_flv_header_flag == FLV_FLAG_AUDIO) {
      if (input_flv_tag->type == FLV_TAG_AUDIO) {
        return 1;
      }
    }
    else if (input_flv_tag->type == FLV_TAG_VIDEO
      && FLV_KEYFRAME == ((flv_tag_video*)(input_flv_tag->data))->video_info.frametype) {
      return 1;
    }
    return 0;
  }

  int32_t FragmentGenerator::set_flv_header(flv_header* input_flv_header, uint32_t flv_header_len) {
    _flv_header_len = flv_header_len;
    if (_flv_header) {
      free(_flv_header);
    }
    _flv_header = (flv_header*)malloc(_flv_header_len);
    ::memcpy(_flv_header, input_flv_header, _flv_header_len);

    _flv_header_flag = _flv_header->flags & FLV_FLAG_BOTH;

    switch (_flv_header_flag)
    {
    case FLV_FLAG_BOTH:
    case FLV_FLAG_AUDIO:
      _main_type = FLV_TAG_AUDIO;
      break;

    case FLV_FLAG_VIDEO:
      _main_type = FLV_TAG_VIDEO;
      break;

    default:
      WRN("set_flv_header failed, stream_id: %s, _flv_header_flag: %d",
        _stream_id.unparse().c_str(), _flv_header_flag);
      return -1;
    }

    INF("set_flv_header success, stream_id: %s, length: %d", _stream_id.unparse().c_str(), _flv_header_len);

    return _flv_header_len;
  }

  FLVMiniBlockGenerator::FLVMiniBlockGenerator(StreamId_Ext& stream_id)
    : FragmentGenerator(stream_id) {
  }

  int32_t FLVMiniBlockGenerator::set_flv_tag(flv_tag* input_flv_tag, bool& generate_fragment) {
    generate_fragment = false;
    int32_t flv_timestamp = flv_get_timestamp(input_flv_tag->timestamp);
    if (flv_timestamp < 0) {
      ERR("flv tag timestamp error, ts: %d, stream_id : %s",
        flv_timestamp, this->_stream_id.unparse().c_str());
      return -1;
    }

    if ((input_flv_tag->type == _main_type) && (flv_timestamp < _last_main_tag_timestamp))
    {
      ERR("detect flv tag timestamp error, ts: %d, _last_ts: %d, stream_id: %s",
        flv_timestamp, _last_main_tag_timestamp, this->_stream_id.unparse().c_str());
    }

    if (input_flv_tag->type == _main_type) {
      _last_main_tag_timestamp = flv_timestamp;
    }

    size_t len = flv_get_datasize(((flv_tag*)input_flv_tag)->datasize) + sizeof(flv_tag)+4;

    if (_input_tag_num == 0) {
      _last_global_seq = _last_seq = -1;
    }

    _generate(input_flv_tag, len);
    generate_fragment = true;

    _input_tag_num++;

    return len;
  }

  FLVMiniBlock* FLVMiniBlockGenerator::get_block() {
    return _block;
  }

  int32_t FLVMiniBlockGenerator::_generate(flv_tag* input_flv_tag, uint32_t tag_len) {
    if (_flv_header_len == 0 || _flv_header == NULL) {
      ERR("_flv_header_len is 0, stream: %s", _stream_id.unparse().c_str());
      assert(0);
      return -1;
    }

    uint32_t input_tag_ts = flv_get_timestamp(input_flv_tag->timestamp);
    uint32_t len = tag_len + sizeof(flv_miniblock_header);

    flv_miniblock_header* block_header = (flv_miniblock_header*)malloc(len);
    _init_block(block_header);
    _last_global_seq++;
    block_header->seq = _last_global_seq;
    block_header->timestamp = input_tag_ts;
    block_header->last_key_seq = _last_key_seq;
    block_header->payload_size = tag_len;
    block_header->av_type = input_flv_tag->type;

    if (_key_type(input_flv_tag) > 0) {
      block_header->key_type = KEY_FRAME_FLV_BLOCK;
      _last_key_seq = block_header->seq;
    }

    memcpy(block_header->payload, input_flv_tag, tag_len);

    FLVMiniBlock* block = new FLVMiniBlock(block_header, false);

    _block = block;

    return 0;
  }

  flv_miniblock_header* FLVMiniBlockGenerator::_init_block(flv_miniblock_header* block) {
    memset(block, 0, sizeof(flv_miniblock_header));
    block->version = FLV_BLOCK_VERSION_2;
    block->format_type = PAYLOAD_TYPE_FLV;
    block->stream_id = _stream_id;
    return block;
  }

  void FLVMiniBlockGenerator::reset_block() {
    _block = NULL;
  }

  FLVMiniBlockGenerator::~FLVMiniBlockGenerator() {
    reset_block();
  }

}

