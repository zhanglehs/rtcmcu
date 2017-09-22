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
#include <utils/memory.h>
#include <util/log.h>
#include <iostream>

using namespace std;
using namespace fragment;
//
//FLVBlock::FLVBlock()
//{
//    _block_buffer = (flv_block_header*)mmalloc(sizeof(flv_block_header));
//    memset(_block_buffer, 0, sizeof(flv_block_header));
//}

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
        if (header == NULL || len < sizeof(flv_header) )
        {
            return;
        }

        _header = (flv_header*)mmalloc(len);
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

        _header = (flv_header*)mmalloc(len);
        _header_len = len;

        memcpy(_header, header, len);

        generate_audio_header();
        generate_video_header();

        return _header;
    }

    flv_header* FLVHeader::generate_audio_header()
    {
        if (_header == NULL || _header_len < sizeof(flv_header) )
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


    FLVBlock::FLVBlock(flv_block_header* input_block_buffer, bool malloc_flag)
    {
        int32_t len = input_block_buffer->payload_size + sizeof(flv_block_header);

        if (malloc_flag)
        {
            _block_buffer = (flv_block_header*)mmalloc(len);
            memcpy(_block_buffer, input_block_buffer, len);
        }
        else
        {
            _block_buffer = input_block_buffer;
        }

   //     printf("create FLVFragment, id: %s, seq: %u\n", get_stream_id().unparse().c_str(), _block_buffer->global_seq);
    }
    //
    //FLVBlock::FLVBlock(vector<flv_tag*>& tag_store)
    //{
    //    int32_t store_size = tag_store.size();
    //    if (store_size == 0)
    //    {
    //        _block_buffer = (flv_block_header*)mmalloc(sizeof(flv_block_header));
    //        memset(_block_buffer, 0, sizeof(flv_block_header));
    //        return;
    //    }
    //
    //    uint32_t payload_size = 0;
    //    for (int32_t i = 0; i < store_size; i++)
    //    {
    //        payload_size += flv_get_datasize(tag_store[i]->datasize) + sizeof(flv_tag)+4;
    //    }
    //
    //    _block_buffer = (flv_block_header*)mmalloc(sizeof(flv_block_header)+payload_size);
    //
    //    memset(_block_buffer, 0, sizeof(flv_block_header));
    //
    //    _block_buffer->payload_size = 0;
    //    for (int32_t i = 0; i < store_size; i++)
    //    {
    //        void* dest = (uint8_t*)_block_buffer + sizeof(flv_block_header)+_block_buffer->payload_size;
    //        void* src = tag_store[i];
    //
    //        flv_tag* tag = (flv_tag*)src;
    //
    //        if (tag->type == FLV_TAG_VIDEO
    //            && ((flv_tag_video*)(tag->data))->video_info.frametype == FLV_KEYFRAME)
    //        {
    //            _block_buffer->frame_type = KEY_FRAME_FLV_BLOCK;
    //        }
    //
    //        int32_t len = flv_get_datasize(tag_store[i]->datasize) + sizeof(flv_tag)+4;
    //        _block_buffer->payload_size += len;
    //
    //        memcpy(dest, src, len);
    //    }
    //
    //    _block_buffer->start_ts = flv_get_timestamp(tag_store[0]->timestamp);
    //    _block_buffer->payload_type = PAYLOAD_TYPE_FLV;
    //}
    //
    StreamId_Ext& FLVBlock::get_stream_id()
    {
        return _block_buffer->stream_id;
    }

    uint32_t FLVBlock::get_start_ts()
    {
        return _block_buffer->start_ts;
    }

    uint32_t FLVBlock::get_end_ts()
    {
        return _block_buffer->end_ts;
    }

    uint32_t FLVBlock::get_seq()
    {
        return _block_buffer->global_seq;
    }

    uint32_t	FLVBlock::get_payload_size()
    {
        return _block_buffer->payload_size;
    }

    string FLVBlock::get_type_s()
    {
        return "FLVBlock";
    }

    uint8_t FLVBlock::get_payload_type()
    {
        return _block_buffer->payload_type;
    }

    uint8_t FLVBlock::get_frame_type()
    {
        return _block_buffer->frame_type;
    }

    bool FLVBlock::is_key()
    {
        return _block_buffer->frame_type == KEY_FRAME_FLV_BLOCK;
    }

    uint32_t FLVBlock::size()
    {
        return _block_buffer->payload_size + sizeof(flv_block_header);
    }

    uint32_t FLVBlock::block_header_len()
    {
        return sizeof(flv_block_header);
    }

    int32_t	FLVBlock::copy_block_header_to_buffer(buffer* dst)
    {
        if (_block_buffer == NULL)
        {
            return 0;
        }
        else
        {
            int32_t len = sizeof(flv_block_header);

            // TODO: buffer no space
            buffer_append_ptr(dst, _block_buffer, len);
        }

        return 0;
    }

    int32_t	FLVBlock::copy_block_to_buffer(buffer* dst)
    {
        if (_block_buffer == NULL)
        {
            return 0;
        }
        else
        {
            int32_t len = _block_buffer->payload_size + sizeof(flv_block_header);

            // TODO: buffer no space
            buffer_append_ptr(dst, _block_buffer, len);
        }

        return 0;
    }

    int32_t	FLVBlock::copy_payload_to_buffer(buffer* dst, uint32_t& timeoffset, uint8_t flv_type_flags, bool quick_start, uint32_t base_timestamp, uint32_t speed_up)
    {
        if (_block_buffer == NULL)
        {
            return 0;
        }
        else
        {
            if (flv_type_flags == FLV_FLAG_BOTH)
            {
                //int32_t len = _block_buffer->payload_size;

                // TODO: buffer no space
                //buffer_append_ptr(dst, _block_buffer->payload, len);
				flv_tag_iterator iterator;
				flv_tag *tag = NULL;

				flv_tag_iterator_init(&iterator,
					(void *)(_block_buffer->payload), _block_buffer->payload_size);
				while (flv_tag_iterate(&iterator, &tag))
				{
#ifdef RESET_TIMESTAMP
					unsigned char *c = (unsigned char *)tag + 4;
					uint32_t src_timestamp = flv_get_timestamp(c);
					if (src_timestamp > 0 && timeoffset == 0)
					{
						timeoffset = src_timestamp;
					}
					uint32_t dst_timestamp = src_timestamp - timeoffset;
					flv_set_timestamp(c, dst_timestamp);
#endif // RESET_TIMESTAMP

                    // debug for yanchangqing
                    // if (FLV_TAG_VIDEO == tag->type)
                    // {
                    //     flv_tag_video* video = (flv_tag_video *)tag->data;
                    //     if (video->video_info.frametype == FLV_KEYFRAME)
                    //     {
                    //         printf("size: %u, timestamp: %lld\n", flv_get_datasize(tag->datasize), flv_get_timestamp(tag->timestamp));
                    //     }
                    // }

                    if (quick_start && FLV_TAG_AUDIO == tag->type) continue;

                    //bool debug_delay_log = false;
                    //if (debug_delay_log && (tag->type == FLV_TAG_VIDEO))
                    //{
                    //    flv_tag_video* video = (flv_tag_video *)tag->data;
                    //    if (video->video_info.frametype == FLV_KEYFRAME)
                    //    {
                    //        timeval now;
                    //        gettimeofday(&now, NULL);

                    //        int64_t timestamp = now.tv_sec * 1000 + now.tv_usec / 1000;

                    //        //    int64_t timestamp = ((int64_t)now.tv_sec) * 1000 + ((int64_t)now.tv_usec) / 1000;
                    //        //   uint32_t murmur = murmur_hash((char*)(tag->data), flv_get_datasize(tag->datasize));
                    //        //char str[1024];
                    //        //printf("delay test. %s timestamp: %lld, datasize: %d\n",
                    //        //    "FLVBlock", timestamp, flv_get_datasize(tag->datasize));
                    //    }
                    //}

                    uint32_t tag_len = sizeof(flv_tag) + flv_get_datasize(tag->datasize) + 4;
                    buffer_append_ptr(dst, tag, tag_len);

                    if (quick_start)
                    {
                        flv_tag* dest_tag = (flv_tag*)(buffer_data_end_ptr(dst) - tag_len);
                        if (base_timestamp == 0 || speed_up == 0)
                        {
                            flv_set_timestamp(dest_tag->timestamp, 0);
                        }
                        else
                        {
                            uint32_t timestamp = flv_get_timestamp(dest_tag->timestamp);
                            uint32_t offset = (base_timestamp - timestamp) / speed_up;
                            flv_set_timestamp(dest_tag->timestamp, base_timestamp - offset);

//                            printf("%u %u %u\n", timestamp, base_timestamp, base_timestamp - offset);
                        }
                    }

				}
            }
            else if (flv_type_flags == FLV_FLAG_AUDIO)
            {
                flv_tag_iterator iterator;
                flv_tag *tag = NULL;

                flv_tag_iterator_init(&iterator,
                    (void *)(_block_buffer->payload), _block_buffer->payload_size);
                while (flv_tag_iterate(&iterator, &tag))
                {
                    if (FLV_TAG_AUDIO != tag->type)
                        continue;
#ifdef RESET_TIMESTAMP
					unsigned char *c = (unsigned char *)tag + 4;
					uint32_t src_timestamp = flv_get_timestamp(c);
					if (src_timestamp > 0 && timeoffset == 0)
					{
						timeoffset = src_timestamp;
					}
					uint32_t dst_timestamp = src_timestamp - timeoffset;
					flv_set_timestamp(c, dst_timestamp);
#endif // RESET_TIMESTAMP
                    buffer_append_ptr(dst, tag, sizeof(flv_tag)+
                        flv_get_datasize(tag->datasize) + 4);
                }
			}
			else if (flv_type_flags == FLV_FLAG_WAIT_VKEY)
            {
				int keyframe_reached = 0;
				flv_tag_iterator iterator;
				flv_tag *tag = NULL;
				flv_tag_iterator_init(&iterator,
					(void *)(_block_buffer->payload), _block_buffer->payload_size);
				while (flv_tag_iterate(&iterator, &tag))
				{
                    if (quick_start && FLV_TAG_AUDIO == tag->type) continue;

					if (!keyframe_reached && FLV_TAG_VIDEO == tag->type)
					{
						if (FLV_KEYFRAME == ((flv_tag_video*)(tag->data))->video_info.frametype) {
							if (1 == ((flv_tag_video*)(tag->data))->avc_packet_type)
							{
								keyframe_reached = 1;
							}
						}
						else {
							continue;
						}
					}

#ifdef RESET_TIMESTAMP
					unsigned char *c = (unsigned char *)tag + 4;
					uint32_t src_timestamp = flv_get_timestamp(c);
					if (src_timestamp > 0 && timeoffset == 0)
					{
						timeoffset = src_timestamp;
					}
					uint32_t dst_timestamp = src_timestamp -timeoffset;
					flv_set_timestamp(c, dst_timestamp);
#endif // RESET_TIMESTAMP

                    uint32_t tag_len = sizeof(flv_tag) + flv_get_datasize(tag->datasize) + 4;
                    buffer_append_ptr(dst, tag, tag_len);

                    if (quick_start)
                    {
                        flv_tag* dest_tag = (flv_tag*)(buffer_data_end_ptr(dst) - tag_len);
                        flv_set_timestamp(dest_tag->timestamp, 0);
                    }

				}
            }
            else
            {

            }
        }

        return 0;
    }


    int32_t FLVBlock::Destroy()
    {
        StreamId_Ext stream_id = get_stream_id();
        string id_str = stream_id.unparse();
 //       printf("destroy FLVFragment, id: %s, seq: %u\n", id_str.c_str(), _block_buffer->global_seq);
        return 0;
    }

    FLVBlock::~FLVBlock()
    {
        if (_block_buffer != NULL)
        {
         //   Destroy();
            mfree(_block_buffer);
            _block_buffer = NULL;
        }

    }

};
