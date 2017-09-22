/****************************************************
 * YueHonghui, 2013-06-02
 * hhyue@tudou.com
 * copyright:youku.com
 * *************************************************/
#ifndef SEGMENT_H_
#define SEGMENT_H_
#include <stdint.h>
#include "common/proto.h"

#if (defined __cplusplus && !defined _WIN32)
extern "C" 
{
#endif
typedef struct media_buffer
{
    uint32_t size;
    uint64_t head;
    uint64_t tail;
    uint32_t media_type;
    media_header *header;
    media_header *audio_header;
    media_header *video_header;
    block_map *bmap[0];
} media_buffer;

media_buffer *media_buffer_create(uint32_t size);

void media_buffer_destroy(media_buffer * m);

int media_buffer_add_header(media_buffer * s, media_header * header);

media_header* gen_audio_header(media_buffer *m);
media_header* gen_video_header(media_buffer *m);

int media_buffer_add_block(media_buffer * s, media_block * data);

const media_header *media_buffer_get_header(media_buffer * s, uint8_t flv_flag);

const block_map *media_buffer_get_last_keyblock(media_buffer * s,
                                                uint64_t * idx);
const block_map *media_buffer_get_next_keyblock(media_buffer * s,
                                                uint64_t * idx);
//@return
//   -1:the req block already fall behind current pool
//   0:the req block success hit
//   1:the req block is not arrived yet
int media_buffer_get_next_block(media_buffer * s, uint64_t * idx,
                                block_map ** block);

const block_map *media_buffer_get_last_block(media_buffer * s,
                                             uint64_t * idx);
#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
