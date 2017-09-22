/*************************************************************
 * YueHonghui, 2013-06-03
 * hhyue@tudou.com
 * copyright:youku.com
 * **********************************************************/

#include "util/common.h"
#include "stream_manager.h"
#include <assert.h>
#include "utils/memory.h"
#include "util/hashtable.h"
#include "util/log.h"
#include "media_buffer.h"
//#include "http_server.h"
#include <json.h>

typedef struct stream
{
    uint32_t streamid;
    media_buffer *media_store;
    hashnode hash_entry;
} stream;

#define MAX_HANDLER (8u)

static hashtable *g_streams = NULL;
static stream_input_handler g_handlers[MAX_HANDLER];
static uint8_t g_watch_type[MAX_HANDLER];
static uint32_t media_buffer_size = 8;

//static void sm_media_buffer_to_json(stream* ss, json_object* json_obj)
//{
//    json_object* ms_size = json_object_new_int(ss->media_store->size);
//    json_object_object_add(json_obj, "ms_size", ms_size);
//
//    json_object* ms_head = json_object_new_int64(ss->media_store->head);
//    json_object_object_add(json_obj, "ms_head", ms_head);
//
//    json_object* ms_tail = json_object_new_int64(ss->media_store->tail);
//    json_object_object_add(json_obj, "ms_tail", ms_tail);
//
//    json_object* ms_media_type = json_object_new_int(ss->media_store->media_type);
//    json_object_object_add(json_obj, "ms_media_type", ms_media_type);
//}

//static void sm_stream_to_json(stream* ss, json_object* json_obj)
//{
//    json_object* jstreamid = json_object_new_int(ss->streamid);
//    json_object_object_add(json_obj, "streamid", jstreamid);
//
//    sm_media_buffer_to_json(ss, json_obj);
//}

static void
notify_watcher(uint32_t streamid, uint8_t watch_type)
{
	uint32_t i = 0;

    for(i = 0; i < MAX_HANDLER; i++) {
        if(g_handlers[i] && g_watch_type[i] & watch_type)
            (g_handlers[i]) (streamid, watch_type);
    }
}

int
stream_manager_init(uint32_t media_buf_sz)
{
	uint32_t i = 0;

    media_buffer_size = media_buf_sz;
    g_streams = hash_create(10);
    if(NULL == g_streams) {
        ERR("g_streams hash_create failed.");
        mfree(g_streams);
        return -1;
    }
    for(i = 0; i < MAX_HANDLER; i++) {
        g_handlers[i] = NULL;
    }

    return 0;
}

int
stream_manager_register_inputed_watcher(stream_input_handler handler,
                                        uint8_t watch_type)
{
    uint32_t i = 0;

    for(; i < MAX_HANDLER; i++) {
        if(NULL == g_handlers[i])
            break;
    }

    if(i >= MAX_HANDLER) {
        WRN("too many handlers, MAX = %u", MAX_HANDLER);
        return -1;
    }

    g_handlers[i] = handler;
    g_watch_type[i] = watch_type;
    return 0;
}

boolean
stream_manager_is_has_stream(uint32_t streamid)
{
    hashnode *node = hash_get(g_streams, streamid);

    if(NULL == node)
        return FALSE;
    return TRUE;
}

int
stream_manager_input_media_header(uint32_t streamid, media_header * header)
{
    int ret = 0;
    hashnode *node = hash_get(g_streams, streamid);

    if(NULL == node) {
        WRN("attempt to input media header whose streamid not exist. streamid = %u", streamid);
        return -1;
    }

    stream *s = container_of(node, stream, hash_entry);

    assert(s->media_store);
    ret = media_buffer_add_header(s->media_store, header);
    if(0 != ret) {
        ERR("media_buffer_add_header failed. ret = %d", ret);
        return -2;
    }
    notify_watcher(streamid, WATCH_HEADER);
    return 0;
}

int
stream_manager_input_media_block(uint32_t streamid, media_block * block)
{
    hashnode *node = hash_get(g_streams, streamid);

    if(NULL == node) {
        WRN("attempt to input media block whose streamid not exist. streamid = %u", streamid);
        return -1;
    }

    stream *s = container_of(node, stream, hash_entry);

    assert(s->media_store);
    int ret = media_buffer_add_block(s->media_store, block);

    if(0 != ret) {
        ERR("media_buffer_add_block failed. ret = %d", ret);
        return -2;
    }
    notify_watcher(streamid, WATCH_BLOCK);
    return 0;
}

int
stream_manager_create_stream(uint32_t streamid)
{
    if(NULL != hash_get(g_streams, streamid))
        return -1;

    stream *s = (stream *) mmalloc(sizeof(stream));

    if(NULL == s) {
        ERR("mmalloc failed. out of memory.");
        return -2;
    }

    s->media_store = media_buffer_create(media_buffer_size);
    if(NULL == s->media_store) {
        ERR("media_buffer_create failed.");
        mfree(s);
        return -3;
    }
    s->streamid = streamid;
    int ret = hash_insert(g_streams, streamid, &s->hash_entry);

    assert(0 == ret);
    return 0;
}

static void
destroy_stream(hashnode * node)
{
    stream *s = container_of(node, stream, hash_entry);

    media_buffer_destroy(s->media_store);
    mfree(s);
}

void
stream_manager_destroy_stream(uint32_t streamid)
{
    hashnode *node = hash_delete(g_streams, streamid);

    if(NULL == node)
        return;

    destroy_stream(node);
}

const block_map *
stream_manager_get_last_keyblock(uint32_t streamid, uint64_t * idx)
{
    hashnode *node = hash_get(g_streams, streamid);

    if(NULL == node) {
        WRN("streamid not exist. streamid = %u", streamid);
        return NULL;
    }

    stream *s = container_of(node, stream, hash_entry);

    return media_buffer_get_last_keyblock(s->media_store, idx);
}

const block_map *
stream_manager_get_next_keyblock(uint32_t streamid, uint64_t * idx)
{
    hashnode *node = hash_get(g_streams, streamid);

    if(NULL == node) {
        WRN("streamid not exist. streamid = %u", streamid);
        return NULL;
    }
    stream *s = container_of(node, stream, hash_entry);

    return media_buffer_get_next_keyblock(s->media_store, idx);
}

const media_header *
stream_manager_get_header(uint32_t streamid, uint8_t flv_flag)
{
    hashnode *node = hash_get(g_streams, streamid);

    if(NULL == node) {
        WRN("attempt to get header whose streamid not exist. streamid = %u",
            streamid);
        return NULL;
    }

    stream *s = container_of(node, stream, hash_entry);

    return media_buffer_get_header(s->media_store, flv_flag);
}

//@return
//    -1: the block requested fall behind the oldest block
//    0 : hit
//    1 : the block requested beyond the newest block
int
stream_manager_get_next_block(uint32_t streamid, uint64_t * idx,
                              block_map ** result)
{
    hashnode *node = hash_get(g_streams, streamid);

    if(NULL == node) {
        WRN("get next block whose streamid not exist. streamid = %u",
            streamid);
        return -2;
    }
    stream *s = container_of(node, stream, hash_entry);

    return media_buffer_get_next_block(s->media_store, idx, result);
}

const block_map *
stream_manager_get_last_block(uint32_t streamid, uint64_t * idx)
{
    hashnode *node = hash_get(g_streams, streamid);

    if(NULL == node) {
        WRN("get last block whose streamid not exist. streamid = %u",
            streamid);
        return NULL;
    }
    stream *s = container_of(node, stream, hash_entry);

    return media_buffer_get_last_block(s->media_store, idx);
}

void
stream_manager_fini()
{
    hash_destroy(g_streams, &destroy_stream);
    g_streams = NULL;
}
