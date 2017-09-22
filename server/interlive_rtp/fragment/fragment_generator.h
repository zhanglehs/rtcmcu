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

#include <common/type_defs.h>
#include <utils/buffer.hpp>
#include "util/flv2ts.h"

#include "fragment.h"
#include "cache_manager_config.h"

#define FLV_BLOCK_VERSION_2 2
#define FLV_FRAGMENT_VERSION_2 2
#define TS_SEGMENT_VERSION_2 2

namespace media_manager
{
    class CacheManager;
}

namespace fragment
{
    class FragmentGenerator
    {
        friend  class media_manager::CacheManager;
    public:
        enum
        {
            CUT_BY_TIME = 1,
            CUT_BY_FRAME = 2
        };
   
        FragmentGenerator(StreamId_Ext& stream_id);

        virtual int32_t set_flv_header(flv_header* input_flv_header, uint32_t flv_header_len);
        virtual int32_t set_flv_tag(flv_tag* input_flv_tag, bool& generate_fragment);

        virtual void reset_tag_buffer();
        virtual void sort_tag_buffer();

        virtual ~FragmentGenerator();

    protected:
        virtual int32_t _cut(flv_tag* input_flv_tag, uint32_t tag_len, uint32_t timestamp);

        virtual int32_t _cut_by_time(flv_tag* input_flv_tag, uint32_t tag_len, uint32_t timestamp);
        virtual int32_t _cut_by_frame(flv_tag* input_flv_tag, uint32_t tag_len, uint32_t timestamp);
        virtual int32_t _generate(flv_tag* input_flv_tag, uint32_t tag_len) = 0;
        virtual int32_t _update_frame_type(flv_tag* input_flv_tag);
        virtual int32_t _key_type(flv_tag* input_flv_tag);

    protected:
        std::deque<flv_tag*> _temp_tag_store;

        buffer* _temp_tag_buf;

        // audio / video
        uint8_t _main_type;

        // cut by timestamp or frametype
        uint8_t _cut_by;

        int32_t _last_main_tag_timestamp;

        uint32_t _last_cut_timestamp;
        timeval _last_cut_timeval;

        uint32_t _keyframe_num;
        uint32_t _cut_interval_ms;

        uint32_t _flv_header_len;
        flv_header* _flv_header;
        uint8_t _flv_header_flag;

        uint32_t _input_tag_num;

        StreamId_Ext _stream_id;

        int32_t _last_seq;
        uint32_t _last_global_seq;
        uint32_t _last_key_seq;

        bool _has_aac0;
        bool _has_avc0;
        bool _has_script_data_object;
    };

    class FLVMiniBlockGenerator : public FragmentGenerator
    {
    public:
        FLVMiniBlockGenerator(StreamId_Ext& stream_id);
        virtual int32_t set_flv_tag(flv_tag* input_flv_tag, bool& generate_fragment);
        FLVMiniBlock* get_block();
        void    reset_block();
        virtual ~FLVMiniBlockGenerator();

    protected:
        virtual int32_t _cut(flv_tag* input_flv_tag, uint32_t tag_len, uint32_t timestamp);
        virtual int32_t _generate(flv_tag* input_flv_tag, uint32_t tag_len);
        flv_miniblock_header* _init_block(flv_miniblock_header* block);

    protected:
        FLVMiniBlock* _block;
    };

    class FLVBlockGenerator : public FragmentGenerator
    {
    public:
        FLVBlockGenerator(StreamId_Ext& stream_id, timeval stream_start_ts);
        std::vector<FLVBlock*>&   get_block();
        void    reset_block();
        virtual ~FLVBlockGenerator();

    protected:
        virtual int32_t _cut(flv_tag* input_flv_tag, uint32_t tag_len, uint32_t timestamp);
        virtual int32_t _generate(flv_tag* input_flv_tag, uint32_t tag_len);
        flv_block_header* _init_block(flv_block_header* block);

    protected:
        std::vector<FLVBlock*> _block_vec;
    };

}


#endif /* __FRAGMENT_GENERATOR_H_ */
