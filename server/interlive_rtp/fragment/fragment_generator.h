/**
* @file fragment_generator.h
* @brief	generator fragments using flv tags. \n
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/11/21
* @see  fragment.h
*/

#ifndef __FRAGMENT_GENERATOR_H_
#define __FRAGMENT_GENERATOR_H_

#include <assert.h>
#include <vector>
#include <deque>

#include <utils/buffer.hpp>
#include "../util/flv2ts.h"

#include "fragment.h"
#include "media_manager/cache_manager_config.h"

#define FLV_BLOCK_VERSION_2 2
#define FLV_FRAGMENT_VERSION_2 2
#define TS_SEGMENT_VERSION_2 2

namespace media_manager
{
  class FlvCacheManager;
}

namespace fragment
{
  class FragmentGenerator {
    friend  class media_manager::FlvCacheManager;

  public:
    FragmentGenerator(StreamId_Ext& stream_id);

    virtual int32_t set_flv_header(flv_header* input_flv_header, uint32_t flv_header_len);

    virtual ~FragmentGenerator();

  protected:
    virtual int32_t _key_type(flv_tag* input_flv_tag);

  protected:
    // audio / video
    uint8_t _main_type;

    int32_t _last_main_tag_timestamp;

    uint32_t _flv_header_len;
    flv_header* _flv_header;
    uint8_t _flv_header_flag;

    uint32_t _input_tag_num;

    StreamId_Ext _stream_id;

    int32_t _last_seq;
    uint32_t _last_global_seq;
    uint32_t _last_key_seq;
  };

  class FLVMiniBlockGenerator : public FragmentGenerator {
  public:
    FLVMiniBlockGenerator(StreamId_Ext& stream_id);
    virtual int32_t set_flv_tag(flv_tag* input_flv_tag, bool& generate_fragment);
    FLVMiniBlock* get_block();
    void    reset_block();
    virtual ~FLVMiniBlockGenerator();

  protected:
    virtual int32_t _generate(flv_tag* input_flv_tag, uint32_t tag_len);
    flv_miniblock_header* _init_block(flv_miniblock_header* block);

  protected:
    FLVMiniBlock* _block;
  };
}

#endif /* __FRAGMENT_GENERATOR_H_ */
