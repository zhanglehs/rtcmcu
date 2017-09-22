/*********************************************************
 * YueHonghui, 2013-06-03
 * hhyue@tudou.com
 * copyright:youku.com
 * *****************************************************/

#ifndef STREAM_MANAGER_H_
#define STREAM_MANAGER_H_
#include <stdint.h>
#include "util/common.h"
#include "common/proto.h"

#if (defined __cplusplus && !defined _WIN32)
extern "C" 
{
#endif
#define WATCH_HEADER 0x01
#define WATCH_BLOCK 0x02

typedef void (*stream_input_handler) (uint32_t streamid, uint8_t watch_type);

int stream_manager_init(uint32_t media_buf_sz);
boolean stream_manager_is_has_stream(uint32_t streamid);
int stream_manager_register_inputed_watcher(stream_input_handler handler,
                                            uint8_t watch_type);
int stream_manager_input_media_header(uint32_t streamid,
                                      media_header * header);
int stream_manager_input_media_block(uint32_t streamid, media_block * block);
int stream_manager_create_stream(uint32_t streamid);
void stream_manager_destroy_stream(uint32_t streamid);
const block_map *stream_manager_get_last_keyblock(uint32_t streamid,
                                                  uint64_t * idx);
const block_map *stream_manager_get_next_keyblock(uint32_t streamid,
                                                  uint64_t * idx);
const media_header *stream_manager_get_header(uint32_t streamid, uint8_t flv_flag);
const block_map *stream_manager_get_last_block(uint32_t streamid,
                                               uint64_t * idx);

//@return
//    -1: the block requested fall behind the oldest block
//    0 : hit
//    1 : the block requested beyond the newest block
// other: error
int stream_manager_get_next_block(uint32_t streamid, uint64_t * idx,
                                  block_map ** result);
void stream_manager_fini();

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
