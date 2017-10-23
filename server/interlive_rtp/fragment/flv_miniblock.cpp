/**
* FLVMiniBlock classes.
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

    FLVMiniBlock::FLVMiniBlock(flv_miniblock_header* input_block_buffer, bool malloc_flag)
    {
        int32_t len = input_block_buffer->payload_size + sizeof(flv_miniblock_header);

        if (malloc_flag)
        {
            _block_buffer = (flv_miniblock_header*)malloc(len);
            memcpy(_block_buffer, input_block_buffer, len);
        }
        else
        {
            _block_buffer = input_block_buffer;
        }
    }
  
    StreamId_Ext& FLVMiniBlock::get_stream_id()
    {
        return _block_buffer->stream_id;
    }

    uint32_t FLVMiniBlock::get_start_ts()
    {
        return _block_buffer->timestamp;
    }

    uint32_t FLVMiniBlock::get_end_ts()
    {
        return _block_buffer->timestamp;
    }

    uint32_t FLVMiniBlock::get_seq()
    {
        return _block_buffer->seq;
    }

    uint32_t	FLVMiniBlock::get_payload_size()
    {
        return _block_buffer->payload_size;
    }

    string FLVMiniBlock::get_type_s()
    {
        return "FLVMiniBlock";
    }

    uint8_t FLVMiniBlock::get_format_type()
    {
        return _block_buffer->format_type;
    }

    uint8_t FLVMiniBlock::get_av_type()
    {
        return _block_buffer->av_type;
    }

    bool FLVMiniBlock::is_key()
    {
        return _block_buffer->key_type == KEY_FRAME_FLV_BLOCK;
    }

    uint32_t FLVMiniBlock::size()
    {
        return _block_buffer->payload_size + sizeof(flv_miniblock_header);
    }

    uint32_t FLVMiniBlock::block_header_len()
    {
        return sizeof(flv_miniblock_header);
    }

    int32_t	FLVMiniBlock::copy_block_header_to_buffer(buffer* dst)
    {
        if (_block_buffer == NULL)
        {
            return 0;
        }
        else
        {
            int32_t len = sizeof(flv_miniblock_header);

            // TODO: buffer no space
            buffer_append_ptr(dst, _block_buffer, len);
        }

        return 0;
    }

    int32_t	FLVMiniBlock::copy_block_to_buffer(buffer* dst)
    {
        if (_block_buffer == NULL)
        {
            return 0;
        }
        else
        {
            int32_t len = _block_buffer->payload_size + sizeof(flv_miniblock_header);

            // TODO: buffer no space
            buffer_append_ptr(dst, _block_buffer, len);
        }

        return 0;
    }

    void print_debug_delay_log(flv_tag* tag)
    {
        //if (tag->type == FLV_TAG_VIDEO)
        //{
        //    flv_tag_video* video = (flv_tag_video *)tag->data;
        //    if (video->video_info.frametype == FLV_KEYFRAME)
        //    {
        //        timeval now;
        //        gettimeofday(&now, NULL);

        //        int64_t timestamp = now.tv_sec * 1000 + now.tv_usec / 1000;

        //        //char str[1024];
        //        //printf("delay test. %s timestamp: %lld, datasize: %d\n",
        //        //    "FLVMiniBlock", timestamp, flv_get_datasize(tag->datasize));
        //    }
        //}
    }

    int32_t	FLVMiniBlock::copy_payload_to_buffer(buffer* dst, uint32_t& timeoffset, uint8_t flv_type_flags, bool quick_start, uint32_t base_timestamp, uint32_t speed_up)
    {
        if (_block_buffer == NULL)
        {
            return 0;
        }

        flv_tag *tag = (flv_tag *)_block_buffer->payload;
        if (flv_type_flags == FLV_FLAG_BOTH)
        {

            bool debug_delay_log = false;
            if (debug_delay_log)
            {
                print_debug_delay_log(tag);
            }
        }
        else if (flv_type_flags == FLV_FLAG_AUDIO)
        {
            if (FLV_TAG_AUDIO != tag->type) return 0;
        }
        else if (flv_type_flags == FLV_FLAG_VIDEO)
        {
            if (FLV_TAG_VIDEO != tag->type) return 0;
        }

        uint32_t tag_len = sizeof(flv_tag) + flv_get_datasize(tag->datasize) + 4;
        buffer_append_ptr(dst, tag, tag_len);

        return 0;
    }


    int32_t FLVMiniBlock::Destroy()
    {
        StreamId_Ext stream_id = get_stream_id();
        string id_str = stream_id.unparse();
        //       printf("destroy FLVFragment, id: %s, seq: %u\n", id_str.c_str(), _block_buffer->global_seq);

        return 0;
    }

    FLVMiniBlock::~FLVMiniBlock()
    {
        if (_block_buffer != NULL)
        {
            //   Destroy();
            free(_block_buffer);
            _block_buffer = NULL;
        }

    }
	time_t BaseBlock::get_active_time()
	{
		return 0;
	}

	BaseBlock::~BaseBlock()
	{
	}

	time_t BaseBlock::set_active()
	{
		return 0;
	}
};
