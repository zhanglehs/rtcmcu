/**
* FLVBlock classes.
* @brief implementation of fragment.h
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/05/22
* @see  fragment.h \n
*/

#include <stdlib.h>
#include <vector>

#include "fragment.h"
#include "../util/log.h"
#include <iostream>

using namespace std;
using namespace fragment;

namespace fragment
{
  FLVHeader::FLVHeader(StreamId_Ext& stream_id)
    :_stream_id(stream_id),
    _header(NULL),
    _header_len(0),
    _audio_config_tag(NULL),
    _audio_header(NULL),
    _audio_header_len(0),
    _video_config_tag(NULL),
    _video_header(NULL),
    _video_header_len(0)
  {

  }

  FLVHeader::FLVHeader(StreamId_Ext& stream_id, flv_header* header, uint32_t len)
    :_stream_id(stream_id),
    _header(NULL),
    _header_len(0),
    _audio_config_tag(NULL),
    _audio_header(NULL),
    _audio_header_len(0),
    _video_config_tag(NULL),
    _video_header(NULL),
    _video_header_len(0)
  {
    if (header == NULL || len < sizeof(flv_header))
    {
      return;
    }

    _header = (flv_header*)malloc(len);
    _header_len = len;

    memcpy(_header, header, len);

    generate_audio_header();
    generate_video_header();
  }

  FLVHeader::~FLVHeader()
  {
    if (_header)
    {
      free(_header);
      _header = NULL;
    }
    if (_audio_header)
    {
      free(_audio_header);
      _audio_header = NULL;
    }
    if (_video_header)
    {
      free(_video_header);
      _video_header = NULL;
    }
    if (_audio_config_tag)
    {
      free(_audio_config_tag);
      _audio_config_tag = NULL;
    }
    if (_video_config_tag)
    {
      free(_video_config_tag);
      _video_config_tag = NULL;
    }
  }

  flv_tag   * FLVHeader::get_audio_config_tag() {
    return _audio_config_tag;
  }
  flv_tag   * FLVHeader::get_video_config_tag() {
    return _video_config_tag;
  }

  flv_header* FLVHeader::set_flv_header(flv_header* header, uint32_t len)
  {
    if (header == NULL || len < sizeof(flv_header))
    {
      return NULL;
    }

    _header = (flv_header*)malloc(len);
    _header_len = len;

    memcpy(_header, header, len);

    generate_audio_header();
    generate_video_header();

    return _header;
  }

  flv_header* FLVHeader::generate_audio_header()
  {
    if (_header == NULL || _header_len < sizeof(flv_header))
    {
      return NULL;
    }

    int parsed = sizeof(flv_header)+4;
    int copied = sizeof(flv_header)+4;

    while (parsed < (int)_header_len)
    {
      flv_tag* tag = (flv_tag*)((uint8_t*)_header + parsed);
      int tag_total_len = flv_get_datasize(tag->datasize) + sizeof(flv_tag)+4;

      int pre_tag_size = flv_get_pretagsize((uint8_t*)_header + parsed +
        flv_get_datasize(tag->datasize) + sizeof(flv_tag));

      if (pre_tag_size != tag_total_len - 4)
      {
        WRN("pre_tag_size != tag_total_len - 4, stream: %s, pre_tag_size: %d, tag_total_len: %d",
          _stream_id.unparse().c_str(), pre_tag_size, tag_total_len);
      }

      if (tag->type == FLV_TAG_AUDIO || tag->type == FLV_TAG_SCRIPT)
      {
        copied += tag_total_len;
      }
      parsed += tag_total_len;
    }

    _audio_header = (flv_header*)malloc(copied);
    _audio_header_len = copied;

    memcpy(_audio_header, _header, sizeof(flv_header)+4);

    parsed = sizeof(flv_header)+4;
    copied = sizeof(flv_header)+4;

    while (parsed < (int)_header_len)
    {
      flv_tag* tag = (flv_tag*)((uint8_t*)_header + parsed);
      uint32_t tag_total_len = flv_get_datasize(tag->datasize) + sizeof(flv_tag)+4;

      //            int pre_tag_size = flv_get_pretagsize((uint8_t*)_header + parsed +
      //                flv_get_datasize(tag->datasize) + sizeof(flv_tag));

      if (tag->type == FLV_TAG_AUDIO || tag->type == FLV_TAG_SCRIPT)
      {
        memcpy((uint8_t*)_audio_header + copied, (uint8_t*)_header + parsed, tag_total_len);
        copied += tag_total_len;
        if (tag->type == FLV_TAG_AUDIO && _audio_config_tag == NULL)
        {
          _audio_config_tag = (flv_tag*)malloc(tag_total_len);
          memcpy((uint8_t*)_audio_config_tag, (uint8_t*)_header + parsed, tag_total_len);
        }
      }
      parsed += tag_total_len;
    }

    _audio_header->flags &= FLV_FLAG_AUDIO;

    return _audio_header;
  }

  flv_header* FLVHeader::generate_video_header()
  {
    if (_header == NULL || _header_len < sizeof(flv_header))
    {
      return NULL;
    }

    uint32_t parsed = sizeof(flv_header)+4;
    uint32_t copied = sizeof(flv_header)+4;

    while (parsed < _header_len)
    {
      flv_tag* tag = (flv_tag*)((uint8_t*)_header + parsed);
      int tag_total_len = flv_get_datasize(tag->datasize) + sizeof(flv_tag)+4;

      int pre_tag_size = flv_get_pretagsize((uint8_t*)_header + parsed +
        flv_get_datasize(tag->datasize) + sizeof(flv_tag));

      if (pre_tag_size != tag_total_len - 4)
      {
        WRN("pre_tag_size != tag_total_len - 4, stream: %s, pre_tag_size: %d, tag_total_len: %d",
          _stream_id.unparse().c_str(), pre_tag_size, tag_total_len);
      }

      if (tag->type == FLV_TAG_VIDEO || tag->type == FLV_TAG_SCRIPT)
      {
        copied += tag_total_len;
      }
      parsed += tag_total_len;
    }

    _video_header = (flv_header*)malloc(copied);
    _video_header_len = copied;

    memcpy(_video_header, _header, sizeof(flv_header)+4);

    parsed = sizeof(flv_header)+4;
    copied = sizeof(flv_header)+4;

    while (parsed < _header_len)
    {
      flv_tag* tag = (flv_tag*)((uint8_t*)_header + parsed);
      uint32_t tag_total_len = flv_get_datasize(tag->datasize) + sizeof(flv_tag)+4;

      //            uint32_t pre_tag_size = flv_get_pretagsize((uint8_t*)_header + parsed +
      //                flv_get_datasize(tag->datasize) + sizeof(flv_tag));

      if (tag->type == FLV_TAG_VIDEO || tag->type == FLV_TAG_SCRIPT)
      {
        memcpy((uint8_t*)_video_header + copied, (uint8_t*)_header + parsed, tag_total_len);
        copied += tag_total_len;
        if (tag->type == FLV_TAG_VIDEO && _video_config_tag == NULL)
        {
          _video_config_tag = (flv_tag*)malloc(tag_total_len);
          memcpy((uint8_t*)_video_config_tag, (uint8_t*)_header + parsed, tag_total_len);
        }
      }
      parsed += tag_total_len;
    }

    _video_header->flags &= FLV_TAG_VIDEO;

    return _video_header;
  }

  flv_header* FLVHeader::get_header(uint32_t& len)
  {
    len = _header_len;
    return _header;
  }

  flv_header* FLVHeader::get_audio_header(uint32_t& len)
  {
    if (_audio_header == NULL || _audio_header_len < sizeof(flv_header))
    {
      generate_audio_header();
    }

    len = _audio_header_len;
    return _audio_header;
  }

  flv_header* FLVHeader::get_video_header(uint32_t& len)
  {
    if (_video_header == NULL || _video_header_len < sizeof(flv_header))
    {
      generate_video_header();
    }

    len = _video_header_len;
    return _video_header;
  }

  int32_t FLVHeader::copy_header_to_buffer(buffer* dst)
  {
    return buffer_append_ptr(dst, _header, _header_len);
  }

  int32_t FLVHeader::copy_audio_header_to_buffer(buffer* dst)
  {
    if (_audio_header == NULL || _audio_header_len < sizeof(flv_header))
    {
      generate_audio_header();
    }

    return buffer_append_ptr(dst, _audio_header, _audio_header_len);
  }

  int32_t FLVHeader::copy_video_header_to_buffer(buffer* dst)
  {
    if (_video_header == NULL || _audio_header_len < sizeof(flv_header))
    {
      generate_video_header();
    }

    return buffer_append_ptr(dst, _video_header, _video_header_len);
  }

};
