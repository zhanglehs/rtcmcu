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

#include "util/log.h"
#include "util/flv.h"
#include "utils/memory.h"
#include "util/flv2ts.h"

#include "common/proto_define.h"
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
        _keyframe_num(0),
        _flv_header(NULL),
        _stream_id(stream_id),
        _has_aac0(false),
        _has_avc0(false),
        _has_script_data_object(false)
    {
        _temp_tag_buf = buffer_create_max(FRAGMENT_MAX_LEN, FRAGMENT_MAX_LEN);
        _flv_header = NULL;
        _last_cut_timestamp = 0;
        _input_tag_num = 0;
    }

    void FragmentGenerator::reset_tag_buffer()
    {
        deque<flv_tag*>::iterator it = _temp_tag_store.begin();
        for (; it != _temp_tag_store.end(); it++)
        {
            delete *it;
        }

        _temp_tag_store.clear();
    }

    static bool less_timestamp(const flv_tag* tag1, const flv_tag* tag2)
    {
        int32_t ts1 = flv_get_timestamp(tag1->timestamp);
        int32_t ts2 = flv_get_timestamp(tag2->timestamp);

        return ts1 < ts2;
    }

    void FragmentGenerator::sort_tag_buffer()
    {
        // need stable sort here because some tags have same timestamp, 
        // but actually we can't change their sequence... I hate backward timestamp !!!!!
        stable_sort(_temp_tag_store.begin(), _temp_tag_store.end(), less_timestamp);
    }

    FragmentGenerator::~FragmentGenerator()
    {
        reset_tag_buffer();

        buffer_free(_temp_tag_buf);

        if (_flv_header != NULL)
        {
            mfree(_flv_header);
            _flv_header = NULL;
        }
    }

    // this is just for backward timestamp !!!!!
    // I hate backward timestamp !!!!
    int32_t FragmentGenerator::set_flv_tag(flv_tag* input_flv_tag, bool& generate_fragment)
    {
        generate_fragment = false;
        int32_t flv_timestamp = flv_get_timestamp(input_flv_tag->timestamp);

        int32_t timestamp = flv_timestamp;

        if (flv_timestamp < 0)
        {
            ERR("flv tag timestamp error, ts: %d, stream_id : %s",
                flv_timestamp, this->_stream_id.unparse().c_str());
            return -1;
        }

        if ((input_flv_tag->type == _main_type) && (flv_timestamp < _last_main_tag_timestamp))
        {
            ERR("detect flv tag timestamp error, ts: %d, _last_ts: %d, stream_id: %s",
                flv_timestamp, _last_main_tag_timestamp, this->_stream_id.unparse().c_str());
         //   return -1;
        }

        if (input_flv_tag->type == _main_type)
        {
            _last_main_tag_timestamp = flv_timestamp;
        }

        size_t len = flv_get_datasize(((flv_tag*)input_flv_tag)->datasize) + sizeof(flv_tag)+4;

        if (_input_tag_num == 0)
        {
            _last_cut_timestamp = timestamp;
            _last_global_seq = _last_seq = -1;
            gettimeofday(&_last_cut_timeval, NULL);
        }

        // input_flv_tag will not be included in latest fragment.
        if (_cut(input_flv_tag, len, timestamp) > 0)
        {
            _generate(input_flv_tag, len);
            generate_fragment = true;
            _keyframe_num = 0;
        }

        uint8_t* temp_tag = new uint8_t[len];

        ::memcpy(temp_tag, input_flv_tag, len);
        _temp_tag_store.push_back((flv_tag*)temp_tag);
        _update_frame_type(input_flv_tag);
        _keyframe_num += _key_type(input_flv_tag);

        _input_tag_num++;

        return len;
    }

    int32_t FragmentGenerator::_cut(flv_tag* input_flv_tag, uint32_t tag_len, uint32_t timestamp)
    {
        if (input_flv_tag->type != _main_type)
        {
            return -1;
        }

        switch (_cut_by)
        {
        case CUT_BY_TIME:
            return _cut_by_time(input_flv_tag, tag_len, timestamp);
        case CUT_BY_FRAME:
            return _cut_by_frame(input_flv_tag, tag_len, timestamp);
        default:
            break;
        }

        return -1;
    }

    int32_t FragmentGenerator::_cut_by_time(flv_tag* input_flv_tag, uint32_t tag_len, uint32_t timestamp)
    {
        if (_cut_interval_ms == 0) return -1;
        if ((timestamp / _cut_interval_ms) != (_last_cut_timestamp / _cut_interval_ms) )
        {
        //    printf("start: %u, end: %u\n", _last_cut_timestamp, timestamp);
            return 1; 
        }
        return -1;
    }

    int32_t FragmentGenerator::_cut_by_frame(flv_tag* input_flv_tag, uint32_t tag_len, uint32_t timestamp)
    {
        if (_cut_by_time(input_flv_tag, tag_len, timestamp) > 0)
        {
            return 1;
        }

        if (input_flv_tag->type == FLV_TAG_VIDEO
            && FLV_KEYFRAME == ((flv_tag_video*)(input_flv_tag->data))->video_info.frametype)
        {
            return 1;
        }
        return -1;
    }

    int32_t FragmentGenerator::_key_type(flv_tag* input_flv_tag)
    {
        if (_flv_header_flag == FLV_FLAG_AUDIO)
        {
            if (input_flv_tag->type == FLV_TAG_AUDIO)
            {
                return 1;
            }
        }
        else if (input_flv_tag->type == FLV_TAG_VIDEO
            && FLV_KEYFRAME == ((flv_tag_video*)(input_flv_tag->data))->video_info.frametype)
        {
            return 1;
        }
        return 0;
    }

    int32_t FragmentGenerator::_update_frame_type(flv_tag* input_flv_tag)
    {
        if (_flv_header_flag == FLV_FLAG_AUDIO)
        {
            if (input_flv_tag->type == FLV_TAG_AUDIO)
            {
                _keyframe_num++;
            }
        }
        else if (input_flv_tag->type == FLV_TAG_VIDEO
            && FLV_KEYFRAME == ((flv_tag_video*)(input_flv_tag->data))->video_info.frametype)
        {
            _keyframe_num++;
        }
        return _keyframe_num;
    }

    int32_t FragmentGenerator::set_flv_header(flv_header* input_flv_header, uint32_t flv_header_len)
    {
        _flv_header_len = flv_header_len;

        if (_flv_header != NULL)
        {
            mfree(_flv_header);
        }

        _flv_header = (flv_header*)mmalloc(_flv_header_len);
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
        : FragmentGenerator(stream_id)
    {
        _cut_by = FragmentGenerator::CUT_BY_TIME;
        _cut_interval_ms = 1;
    }

    int32_t FLVMiniBlockGenerator::set_flv_tag(flv_tag* input_flv_tag, bool& generate_fragment)
    {
        generate_fragment = false;
        int32_t flv_timestamp = flv_get_timestamp(input_flv_tag->timestamp);

        if (flv_timestamp < 0)
        {
            ERR("flv tag timestamp error, ts: %d, stream_id : %s",
                flv_timestamp, this->_stream_id.unparse().c_str());
            return -1;
        }

        if ((input_flv_tag->type == _main_type) && (flv_timestamp < _last_main_tag_timestamp))
        {
            ERR("detect flv tag timestamp error, ts: %d, _last_ts: %d, stream_id: %s",
                flv_timestamp, _last_main_tag_timestamp, this->_stream_id.unparse().c_str());
        }

        if (input_flv_tag->type == _main_type)
        {
            _last_main_tag_timestamp = flv_timestamp;
        }

        size_t len = flv_get_datasize(((flv_tag*)input_flv_tag)->datasize) + sizeof(flv_tag) + 4;

        if (_input_tag_num == 0)
        {
            _last_cut_timestamp = flv_timestamp;
            _last_global_seq = _last_seq = -1;
        }

        _generate(input_flv_tag, len);
        generate_fragment = true;

        _input_tag_num++;

        return len;
    }

    FLVMiniBlock* FLVMiniBlockGenerator::get_block()
    {
        return _block;
    }

    int32_t FLVMiniBlockGenerator::_cut(flv_tag* input_flv_tag, uint32_t tag_len, uint32_t timestamp)
    {
        return 1;
    }

    int32_t FLVMiniBlockGenerator::_generate(flv_tag* input_flv_tag, uint32_t tag_len)
    {
        if (_flv_header_len == 0 || _flv_header == NULL)
        {
            ERR("_flv_header_len is 0, stream: %s", _stream_id.unparse().c_str());
            assert(0);
            return -1;
        }

        uint32_t input_tag_ts = flv_get_timestamp(input_flv_tag->timestamp);
        uint32_t len = tag_len + sizeof(flv_miniblock_header);

        flv_miniblock_header* block_header = (flv_miniblock_header*)mmalloc(len);
        _init_block(block_header);
        _last_global_seq++;
        block_header->seq = _last_global_seq;
        block_header->timestamp = input_tag_ts;
        block_header->last_key_seq = _last_key_seq;
        block_header->payload_size = tag_len;
        block_header->av_type = input_flv_tag->type;

        if (_key_type(input_flv_tag) > 0)
        {
            block_header->key_type = KEY_FRAME_FLV_BLOCK;
            _last_key_seq = block_header->seq;
            _keyframe_num++;
        }

        memcpy(block_header->payload, input_flv_tag, tag_len);

        FLVMiniBlock* block = new FLVMiniBlock(block_header, false);

        _block = block;

        return 0;
    }

    flv_miniblock_header* FLVMiniBlockGenerator::_init_block(flv_miniblock_header* block)
    {
        memset(block, 0, sizeof(flv_miniblock_header));
        block->version = FLV_BLOCK_VERSION_2;
        block->format_type = PAYLOAD_TYPE_FLV;
        block->stream_id = _stream_id;
        return block;
    }

    void FLVMiniBlockGenerator::reset_block()
    {
        _block = NULL;
    }

    FLVMiniBlockGenerator::~FLVMiniBlockGenerator()
    {
        reset_block();
    }


    FLVBlockGenerator::FLVBlockGenerator(StreamId_Ext& stream_id, timeval stream_start_ts)
        : FragmentGenerator(stream_id)
    {
        _cut_by = FragmentGenerator::CUT_BY_TIME;
        _cut_interval_ms = 100;
    }

    std::vector<FLVBlock*>& FLVBlockGenerator::get_block()
    {
        return _block_vec;
    }

    int32_t FLVBlockGenerator::_cut(flv_tag* input_flv_tag, uint32_t tag_len, uint32_t timestamp)
    {
        if (_temp_tag_store.size() > 100)
        {
            WRN("_temp_tag_store.size() is too large, force cut. stream_id: %s", _stream_id.unparse().c_str());
            return 1;
        }

        if (input_flv_tag->type != _main_type)
        {
            return -1;
        }

        return _cut_by_time(input_flv_tag, tag_len, timestamp);
    }

    int32_t FLVBlockGenerator::_generate(flv_tag* input_flv_tag, uint32_t tag_len)
    {
        if (_flv_header_len == 0 || _flv_header == NULL)
        {
            ERR("_flv_header_len is 0, stream: %s", _stream_id.unparse().c_str());
            assert(0);
            return -1;
        }

        // sort_tag_buffer();

        uint32_t input_tag_ts = flv_get_timestamp(input_flv_tag->timestamp);

        int32_t store_id = 0;
        buffer_reset(_temp_tag_buf);

        uint32_t start_ts = 0;
        uint32_t end_ts = 0;

        for (store_id = 0; store_id < (int)_temp_tag_store.size(); store_id++)
        {
            flv_tag* tag = _temp_tag_store[store_id];
            // int32_t timestamp = flv_get_timestamp(tag->timestamp);

            // if (timestamp / (int32_t)_cut_interval_ms < block_seq)
            // {
            //     WRN("drop tag by timestamp, stream: %s, ts: %d, block: %d",
            //         _stream_id.unparse().c_str(), timestamp, block_seq);
            //     //continue;
            // }
            // 
            // if (timestamp / (int32_t)_cut_interval_ms > block_seq)
            // {
            //     break;
            // }

            // printf("ts: %u, type: %u\n", flv_get_timestamp(tag->timestamp), tag->type);

            if (tag->type == _main_type)
            {
                if (start_ts == 0)
                {
                    start_ts = flv_get_timestamp(tag->timestamp);
                }
                end_ts = flv_get_timestamp(tag->timestamp);
            }

//            printf("ts: %u, type: %u ", flv_get_timestamp(tag->timestamp), tag->type);

            int32_t tag_len = flv_get_datasize(tag->datasize) + sizeof(flv_tag) + 4;
            buffer_append_ptr(_temp_tag_buf, tag, tag_len);
        }

//        printf("\n");

        if (start_ts == 0)
        {
            flv_tag* tag = _temp_tag_store[0];
            start_ts = flv_get_timestamp(tag->timestamp);
        }

        // TODO: merge 2 loops
        for (int32_t id = 0; id < store_id; id++)
        {
            flv_tag* tag = _temp_tag_store.front();
            delete tag;
            _temp_tag_store.pop_front();
        }

        uint32_t payload_len = buffer_data_len(_temp_tag_buf);
        uint32_t len = payload_len + sizeof(flv_block_header);

        flv_block_header* block_header = (flv_block_header*)mmalloc(len);
        _init_block(block_header);

        _last_global_seq++;
        block_header->global_seq = _last_global_seq;

        block_header->start_ts = start_ts;
        block_header->end_ts = end_ts;

        block_header->last_key_block_global_seq = _last_key_seq;
        block_header->payload_size = payload_len;

        if (_keyframe_num > 0)
        {
            block_header->frame_type = KEY_FRAME_FLV_BLOCK;
            _last_key_seq = block_header->global_seq;
        }

        FLVBlock* block = new FLVBlock(block_header, false);

        _block_vec.push_back(block);

        _last_cut_timestamp = input_tag_ts;

        return 0;
    }

    flv_block_header* FLVBlockGenerator::_init_block(flv_block_header* block)
    {
        memset(block, 0, sizeof(flv_block_header));
        block->version = FLV_BLOCK_VERSION_2;
        block->payload_type = PAYLOAD_TYPE_FLV;
        block->stream_id = _stream_id;

        memcpy(block->payload, buffer_data_ptr(_temp_tag_buf), buffer_data_len(_temp_tag_buf));

        return block;
    }

    void FLVBlockGenerator::reset_block()
    {
        _block_vec.clear();
    }

    FLVBlockGenerator::~FLVBlockGenerator()
    {
        vector<FLVBlock*>::iterator it = _block_vec.begin();
        for (; it != _block_vec.end(); it++)
        {
            delete *it;
        }

        reset_block();
    }
}

