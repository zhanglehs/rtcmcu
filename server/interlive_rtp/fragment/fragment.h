/**
 * @file fragment.h
 * @brief	This file provide 3 base data structure for stream transport and storage. \n
 FLVBlock, FLVFragment and TSSegment.
 * @author songshenyi
 * <pre><b>copyright: Youku</b></pre>
 * <pre><b>email: </b>songshenyi@youku.com</pre>
 * <pre><b>company: </b>http://www.youku.com</pre>
 * <pre><b>All rights reserved.</b></pre>
 * @date 2014/05/22
 *
 */

#ifndef __FRAGMENT_H__
#define __FRAGMENT_H__

#include <vector>
#include <list>

#include <uuid/uuid.h>

#include "util/flv.h"
#include "utils/buffer.hpp"
#include "streamid.h"

#include <sys/time.h>
#include <time.h>

#ifdef _WIN32
#define NULL 0
#endif

namespace media_manager
{
  class FlvCacheManager;
}

namespace fragment
{

  static const uint32_t CutIntervalForBlock = 100; // millisecond
  static const uint32_t CutIntervalForFragment = 10000; // 10 seconds

  enum FrameType
  {
    NORMAL_FRAME_FLV_BLOCK = 0,
    KEY_FRAME_FLV_BLOCK = 1
  };

#pragma pack(1)

  typedef struct
  {
    uint32_t version;
    StreamId_Ext stream_id;
    uint32_t timestamp;
    int64_t stream_start_ts_msec;
    uint32_t seq;
    uint32_t last_key_seq;
    uint32_t payload_size;

    uint8_t format_type;
    uint8_t av_type;
    uint8_t key_type;

    uint8_t payload[0];
  }flv_miniblock_header;

#pragma pack()


  class BaseBlock
  {
  public:
    virtual uint32_t get_seq() = 0;
    virtual uint32_t get_start_ts() = 0;
    virtual uint32_t get_end_ts() = 0;
    virtual StreamId_Ext& get_stream_id() = 0;
    virtual uint32_t size() = 0;
    virtual std::string get_type_s() = 0;
    virtual bool is_key() = 0;

    virtual time_t        get_active_time();
    virtual time_t        set_active();

    virtual ~BaseBlock();
  };

  class FLVMiniBlock : public BaseBlock
  {
    friend class media_manager::FlvCacheManager;
  public:
    FLVMiniBlock(flv_miniblock_header* input_block_buffer, bool malloc_flag = true);
    uint32_t		get_seq();
    uint32_t		get_start_ts();
    uint32_t		get_end_ts();
    StreamId_Ext&	get_stream_id();
    uint32_t		size();
    std::string get_type_s();
    bool is_key();

    uint32_t		get_payload_size();
    uint8_t			get_format_type();
    uint8_t			get_av_type();
    uint32_t		block_header_len();

    // these functions used by module_player
    int32_t			copy_block_header_to_buffer(buffer* dst);
    int32_t			copy_block_to_buffer(buffer* dst);
    int32_t	copy_payload_to_buffer(buffer* dst, uint32_t& timeoffset, uint8_t flv_type_flags = FLV_FLAG_BOTH, bool quick_start = false, uint32_t base_timestamp = 0, uint32_t speed_up = 0);

    int32_t Destroy();
    ~FLVMiniBlock();

  protected:
    flv_miniblock_header* _block_buffer;
  };

  class FLVHeader
  {
  public:
    FLVHeader(StreamId_Ext& stream_id);
    FLVHeader(StreamId_Ext& stream_id, flv_header* header, uint32_t len);
    ~FLVHeader();
    flv_header* set_flv_header(flv_header* header, uint32_t len);

    flv_header* generate_audio_header();
    flv_header* generate_video_header();

    flv_header* get_header(uint32_t& len);
    flv_header* get_audio_header(uint32_t& len);
    flv_header* get_video_header(uint32_t& len);

    flv_tag   * get_audio_config_tag();
    flv_tag   * get_video_config_tag();

    int32_t copy_header_to_buffer(buffer* dst);
    int32_t copy_audio_header_to_buffer(buffer* dst);
    int32_t copy_video_header_to_buffer(buffer* dst);

  protected:
    StreamId_Ext _stream_id;

    flv_header* _header;
    uint32_t    _header_len;

    flv_tag *   _audio_config_tag;
    flv_header* _audio_header;
    uint32_t    _audio_header_len;

    flv_tag *   _video_config_tag;
    flv_header* _video_header;
    uint32_t    _video_header_len;
  };

}
#endif /* __FRAGMENT_H__ */

