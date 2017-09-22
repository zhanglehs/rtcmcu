/*************************************************
 * YueHonghui, 2013-06-02
 * hhyue@tudou.com
 * copyright:youku.com
 * ***********************************************/

#include "stdlib.h"
#include "media_buffer.h"
#include <assert.h>
#include <string.h>
#include <arpa/inet.h>
#include "utils/memory.h"
#include "util/log.h"
#include "util/flv.h"
#include "common/proto.h"

media_buffer *
media_buffer_create(uint32_t size)
{
    uint32_t i = 0;
    media_buffer *mb =
        (media_buffer *) mmalloc(sizeof(media_buffer) +
                                 size * sizeof(block_map *));
    if(NULL == mb) {
        ERR("mmalloc failed.");
        return NULL;
    }
    mb->size = size;
    mb->head = mb->tail = 0;
    mb->header = NULL;
    mb->audio_header = NULL;
    mb->video_header = NULL;
    for(i = 0; i < size; i++)
        mb->bmap[i] = NULL;

    return mb;
}

void
media_buffer_destroy(media_buffer * m)
{
    uint32_t i = 0;

    if (m != NULL)
    {
        if (m->header == NULL)
        {
            INF("media_buffer_destroy, header=NULL");
        }
        else
        {
            INF("media_buffer_destroy, streamid=%d", m->header->streamid);
        }
    }
    else
    {
        ERR("media_buffer is NULL in media_buffer_destroy");
    }

    assert(m);
    if (m->header)
    {
        mfree(m->header);
        m->header = NULL;
    }

    if (m->audio_header)
    {
        mfree(m->audio_header);
        m->audio_header = NULL;
    }

    if (m->video_header)
    {
        mfree(m->video_header);
        m->video_header = NULL;
    }

    for(i = 0; i < m->size; i++) {
        if(m->bmap[i])
            mfree(m->bmap[i]);
    }

    mfree(m);
}

int
media_buffer_add_block(media_buffer * m, media_block * data)
{
    if(!m->header && data->payload_type <= PAYLOAD_TYPE_MEDIA_END) {
        ERR("attempt to add block before header arrived.");
        return -1;
    }

    uint32_t pre_tag_sz = 0;
    int32_t last_key_ts = -1;
    int32_t last_ts = -1;
    uint32_t last_key_start = 0;

    if(PAYLOAD_TYPE_FLV == data->payload_type) {
        uint8_t *pos = data->payload + data->payload_size;
        flv_tag *tag = NULL;
        flv_tag_video *video = NULL;

        uint64_t parsed_len = 0;
        while(pos > data->payload) {
            pos -= 4;
            parsed_len += 4;
            pre_tag_sz = ntohl(*(uint32_t *) pos);
            parsed_len += pre_tag_sz;
            pos -= pre_tag_sz;
            if (pos < data->payload || parsed_len> data->payload_size) {
                WRN("stream invalid, stream: %u, pre_tag_sz: %u", data->streamid, pre_tag_sz);
                return -2;
            }
            tag = (flv_tag *) pos;
            if(FLV_FLAG_AUDIO == m->media_type && FLV_TAG_AUDIO == tag->type) {
                if(-1 == last_key_ts){
                    last_key_ts = flv_get_timestamp(tag->timestamp);
                    last_key_start = pos - data->payload;
                }
                if(-1 == last_ts)
                    last_ts = last_key_ts;
                break;
            }
            if(FLV_FLAG_AUDIO != m->media_type && FLV_TAG_VIDEO == tag->type) {
                if(-1 == last_ts)
                    last_ts = flv_get_timestamp(tag->timestamp);
                video = (flv_tag_video *) tag->data;
                if(-1 == last_key_ts && FLV_KEYFRAME == video->video_info.frametype) {
                    last_key_ts = flv_get_timestamp(tag->timestamp);
                    last_key_start = pos - data->payload;
                    break;
                }
            }
        }
    }
//    DBG("payload_sz = %u, last_key_ts = %d, last_key_start = %u, seq = %lu", 
//            data->payload_size, last_key_ts, last_key_start, data->seq);

    if((int64_t) m->tail - (int64_t) m->head < (int) m->size) {
        block_map *bm =
            (block_map *) mmalloc(sizeof(block_map) + data->payload_size);
        if(NULL == bm)
            return -3;

        m->bmap[m->tail % m->size] = bm;
        memcpy(&bm->data, data, sizeof(media_block) + data->payload_size);
        bm->last_keyframe_ts = last_key_ts;
        bm->last_keyframe_start = last_key_start;
        bm->last_ts = last_ts;
        m->tail++;
    }

    if(m->tail - m->head >= m->size) {
        mfree(m->bmap[m->head % m->size]);
        m->bmap[m->head % m->size] = NULL;
        m->head++;
    }
    return 0;
}

media_header* gen_video_header(media_buffer * m)
{
    if (m->video_header != NULL)
    {
        return m->video_header;
    }

    if (m->header == NULL)
    {
        m->video_header = NULL;
        return NULL;
    }

    flv_header* header = (flv_header*)malloc(m->header->payload_size);
    memcpy(header, m->header->payload, sizeof(flv_header)+4);

    header->flags = FLV_TAG_VIDEO;

    uint32_t parsed = sizeof(flv_header)+4;
    uint32_t copied = sizeof(flv_header)+4;
    while (parsed < m->header->payload_size)
    {
        flv_tag* tag = (flv_tag*)(m->header->payload + parsed);
        uint32_t tag_total_len = flv_get_datasize(tag->datasize) + sizeof(flv_tag)+4;

        uint32_t pre_tag_size = flv_get_pretagsize(m->header->payload + parsed + flv_get_datasize(tag->datasize) + sizeof(flv_tag));

        if (pre_tag_size != tag_total_len - 4)
        {
            ERR("pre_tag_size != tag_total_len - 4, stream: %d, pre_tag_size: %d, tag_total_len: %d",
                m->header->streamid, pre_tag_size, tag_total_len);
            memcpy(header, m->header->payload, m->header->payload_size);
            copied = m->header->payload_size;
            break;
        }

        if (tag->type == FLV_TAG_VIDEO || tag->type == FLV_TAG_SCRIPT)
        {
            memcpy((char*)header + copied, m->header->payload + parsed, tag_total_len);
            copied += tag_total_len;
        }
        parsed += tag_total_len;
    }

    m->video_header = (media_header*)malloc(copied + sizeof(media_header));
    m->video_header->streamid = m->header->streamid;
    m->video_header->payload_type = PAYLOAD_TYPE_FLV;
    m->video_header->payload_size = copied;

    memcpy(m->video_header->payload, header, copied);

    free(header);

    return m->video_header;
}

media_header* gen_audio_header(media_buffer * m)
{
    if (m->audio_header != NULL)
    {
        return m->audio_header;
    }

    if (m->header == NULL)
    {
        m->audio_header = NULL;
        return NULL;
    }

    flv_header* header = (flv_header*)malloc(m->header->payload_size);
    memcpy(header, m->header->payload, sizeof(flv_header)+4);

    header->flags = FLV_FLAG_AUDIO;

    uint32_t parsed = sizeof(flv_header)+4;
    uint32_t copied = sizeof(flv_header)+4;
    while (parsed < m->header->payload_size)
    {
        flv_tag* tag = (flv_tag*)(m->header->payload + parsed);
        uint32_t tag_total_len = flv_get_datasize(tag->datasize) + sizeof(flv_tag)+4;

        uint32_t pre_tag_size = flv_get_pretagsize(m->header->payload + parsed + flv_get_datasize(tag->datasize) + sizeof(flv_tag));

        if (pre_tag_size != tag_total_len - 4)
        {
            ERR("pre_tag_size != tag_total_len - 4, stream: %d, pre_tag_size: %d, tag_total_len: %d",
                m->header->streamid, pre_tag_size, tag_total_len);
            memcpy(header, m->header->payload, m->header->payload_size);
            copied = m->header->payload_size;
            break;
         }

        if (tag->type == FLV_TAG_AUDIO || tag->type == FLV_TAG_SCRIPT)
        {
            memcpy((char*)header + copied, m->header->payload + parsed, tag_total_len);
            copied += tag_total_len;
        }
        parsed += tag_total_len;
    }

    m->audio_header = (media_header*)malloc(copied + sizeof(media_header));
    m->audio_header->streamid = m->header->streamid;
    m->audio_header->payload_type = PAYLOAD_TYPE_FLV;
    m->audio_header->payload_size = copied;

    memcpy(m->audio_header->payload, header, copied);

    free(header);

    return m->audio_header;
}

const media_header *media_buffer_get_header(media_buffer * m, uint8_t flv_flag)
{
    if (flv_flag == FLV_FLAG_BOTH)
    {
        return m->header; 
    }

    if (flv_flag == FLV_FLAG_AUDIO)
    {
        if (m->audio_header != NULL)
        {
            return m->audio_header;
        }

        return gen_audio_header(m);
    }

    if (flv_flag == FLV_FLAG_VIDEO)
    {
        if (m->video_header != NULL)
        {
            return m->video_header;
        }

        return gen_video_header(m);
    }

    return NULL;
}

int
media_buffer_add_header(media_buffer * m, media_header * header)
{
    INF("media_buffer_add_header, streamid=%d, header_len=%d", 
        header->streamid, header->payload_size);

    if(PAYLOAD_TYPE_FLV != header->payload_type) {
        WRN("unsupported payload_type = %hu",
            (unsigned short) header->payload_type);
        return -1;
    }
    media_header *mh =
        (media_header *) mmalloc(sizeof(media_header) + header->payload_size);
    if(NULL == mh) {
        ERR("mmalloc media_header failed. out of memory!");
        return -2;
    }
    memcpy(mh, header, sizeof(media_header) + header->payload_size);
    if (m->header)
    {
        mfree(m->header);
        m->header = NULL;
    }

    if (m->audio_header)
    {
        mfree(m->audio_header);
        m->audio_header = NULL;
    }

    if (m->video_header)
    {
        mfree(m->video_header);
        m->video_header = NULL;
    }

    m->header = mh;
    flv_header *fh = (flv_header *) mh->payload;

    m->media_type = (uint32_t) fh->flags;
    return 0;
}

const block_map *
media_buffer_get_last_keyblock(media_buffer * m, uint64_t * idx)
{
    if(m->head == m->tail)
        return NULL;
    uint64_t i = m->tail;
    block_map *block = NULL;

    for(; i > m->head; i--) {
        block = m->bmap[(i - 1) % m->size];
        if(-1 != block->last_keyframe_ts) {
            if(idx)
                *idx = i;
            //    DBG("get last keyblock success. payload_sz = %u, last_key_ts = %d, last_key_start = %u, seq = %lu",
            //          block->data.payload_size, block->last_keyframe_ts, block->last_keyframe_start, block->data.seq);
            return block;
        }
    }
    return NULL;
}

const block_map *
media_buffer_get_next_keyblock(media_buffer * m, uint64_t * idx)
{
    if(m->head == m->tail)
        return NULL;
    uint64_t start = NULL == idx ? m->head : *idx;
    uint64_t i = MAX(m->head, start);
    block_map *block = NULL;

    for(; i < m->tail; i++) {
        block = m->bmap[i % m->size];
        if(-1 != block->last_keyframe_ts) {
            if(idx)
                *idx = i + 1;
            return block;
        }
    }
    return NULL;
}

const block_map *
media_buffer_get_last_block(media_buffer * m, uint64_t * idx)
{
    if(m->head == m->tail)
        return NULL;
    uint64_t i = m->tail;
    block_map *block = NULL;

    for(; i > m->head; i--) {
        block = m->bmap[(i - 1) % m->size];
        if(PAYLOAD_TYPE_FLV == block->data.payload_type) {
            if(idx)
                *idx = i;
            //    DBG("get last keyblock success. payload_sz = %u, last_key_ts = %d, last_key_start = %u, seq = %lu",
            //          block->data.payload_size, block->last_keyframe_ts, block->last_keyframe_start, block->data.seq);
            return block;
        }
    }
    return NULL;
}

int
media_buffer_get_next_block(media_buffer * m, uint64_t * idx,
                            block_map ** result)
{
    if(m->head == m->tail)
        return -2;
    if(*idx < m->head)
        return -1;
    if(*idx >= m->tail)
        return 1;

    *result = m->bmap[*idx % m->size];
    (*idx)++;
    return 0;
}
