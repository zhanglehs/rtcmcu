/*
 * stream_recorder.c
 *
 *  Created on: 2013-8-13
 *      Author: zzhang
 */  
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "stream_recorder.h"
    
#include <assert.h>
#include <errno.h>
#include <event.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/memory.h"
#include "util/access.h"
#include "util/util.h"
#include "util/list.h"
#include "util/common.h"
#include "utils/buffer.hpp"
#include "util/flv.h"
#include "util/log.h"
#include "util/hashtable.h"
#include "stream_manager.h"
typedef enum 
{ 
    RECORD_STREAM_HEADER = 1, 
    RECORD_STREAM_FIRST_TAG = 2, 
    RECORD_STREAM_TAG = 3, 
} record_state;
typedef struct rec_file 
{
    uint32_t streamid;
    FILE * file;
    record_state state;
    uint64_t idx;
    hashnode hash_entry;
} rec_file;
static char g_record_dir[PATH_MAX];
static hashtable *g_rec_files = NULL;

rec_file * get_rec_file(uint32_t streamid) 
{
    hashnode * node = hash_get(g_rec_files, streamid);
    if(node == NULL)
        return NULL;
    return container_of(node, rec_file, hash_entry);
}

void
close_rec_file(rec_file * rf) 
{
    fflush(rf->file);
    fclose(rf->file);
    hash_delete(g_rec_files, rf->streamid);
    mfree(rf);
    rf = NULL;
} 
static int
record_block(FILE * file, const block_map * block, int block_start) 
{
    const size_t sz = block->data.payload_size - block_start;

    if(fwrite(block->data.payload + block_start, 1, sz, file) < sz) {
        ERR("fwrite error:%d", ferror(file));
        return -1;
    }
    return 0;
}

static void
recorder_on_header(uint32_t streamid, uint8_t watch_type) 
{
    INF("recorder_on_header,streamid:%u", streamid);
    rec_file * rf = get_rec_file(streamid);
    if(rf == NULL) {
        //        WRN("recorder_on_header,no such stream:%u",streamid);
        start_record_stream(streamid);
        rf = get_rec_file(streamid);
        if(rf == NULL) {
            WRN("start_record_stream error");
            return;
        }
    }
    if(rf->state != RECORD_STREAM_HEADER) {
        DBG("already recorded header,streamid:%u", streamid);
        return;
    }
    const media_header *header = stream_manager_get_header(streamid, FLV_FLAG_BOTH);

    if(NULL == header) {
        ERR("no header,streamid:%u", streamid);
        close_rec_file(rf);
        return;
    }
    if(fwrite(header->payload, 1, header->payload_size, rf->file) 
        <header->payload_size) {
        close_rec_file(rf);
        return;
    }
    rf->state = RECORD_STREAM_FIRST_TAG;
}

static void
record_first_tag(rec_file * rf) 
{
    uint64_t idx = 0;
    const block_map *block =
        stream_manager_get_last_keyblock(rf->streamid, &idx);
    if(NULL != block) {
        if(0 == record_block(rf->file, block, block->last_keyframe_start)) {
            rf->state = RECORD_STREAM_TAG;
            rf->idx = idx;
        }
        
        else {
            WRN("record_first_tag error,streamid:%u", rf->streamid);
            close_rec_file(rf);
        }
    }
}

static void
record_tag(rec_file * rf) 
{
    int ret = 0;

    block_map * block = NULL;
    uint64_t idx = rf->idx;
    ret = stream_manager_get_next_block(rf->streamid, &idx, &block);
    if(ret == 0 && PAYLOAD_TYPE_FLV == block->data.payload_type) {
        if(0 == record_block(rf->file, block, 0)) {
            rf->idx = idx;
        }
        
        else {
            WRN("record_tag error");
        }
    }
}

static void
recorder_on_block(uint32_t streamid, uint8_t watch_type) 
{
    DBG("recorder_on_header,streamid:%u", streamid);
    rec_file * rf = get_rec_file(streamid);
    if(rf == NULL) {
        WRN("recorder_on_block,no such stream:%u", streamid);
        return;
    }
    if(rf->state == RECORD_STREAM_FIRST_TAG) {
        record_first_tag(rf);
    }
    
    else if(rf->state == RECORD_STREAM_TAG) {
        record_tag(rf);
    }
}

int
stream_recorder_init(const char *record_dir) 
{
    memset(g_record_dir, 0, sizeof(PATH_MAX));
    if(NULL == realpath(record_dir, g_record_dir)) {
        fprintf(stderr, "realpath failed. record_dir = %s, error = %s\n",
                 record_dir, strerror(errno));
        return -1;
    }
    g_rec_files = hash_create(10);
    if(NULL == g_rec_files) {
        ERR("g_rec_files hash_create failed.");
        mfree(g_rec_files);
        return -1;
    }
    stream_manager_register_inputed_watcher(recorder_on_header, WATCH_BLOCK 
                                             |WATCH_HEADER);
    stream_manager_register_inputed_watcher(recorder_on_block, WATCH_BLOCK);
    return 0;
}

void
close_rec_file_node(hashnode * node) 
{
    rec_file * rf = container_of(node, rec_file, hash_entry);
    fflush(rf->file);
    fclose(rf->file);
}

void
stream_recorder_fini() 
{
    hash_destroy(g_rec_files, close_rec_file_node);
}

void
start_record_stream(uint32_t streamid) 
{
    INF("start_record_stream:%u", streamid);
    rec_file * rf = get_rec_file(streamid);
    if(rf != NULL) {
        fflush(rf->file);
        fclose(rf->file);
    }
    
    else {
        rf = (rec_file *)mmalloc(sizeof(rec_file));
        if(rf == NULL) {
            ERR("mmalloc rec_file error");
            return;
        }
        hash_insert(g_rec_files, streamid, &rf->hash_entry);
    }
    rf->streamid = streamid;
    rf->state = RECORD_STREAM_HEADER;
    rf->idx = 0;
    struct tm curr_tm;

    time_t t = time(NULL);
    if(NULL == localtime_r(&t, &curr_tm)) {
        return;
    }
    char filename[PATH_MAX];

    memset(filename, 0, PATH_MAX);
    snprintf(filename, PATH_MAX - 1, "%s/%u-%04d%02d%02d-%02d%02d%02d.flv",
              g_record_dir, streamid, curr_tm.tm_year + 1900,
              curr_tm.tm_mon + 1, curr_tm.tm_mday, curr_tm.tm_hour,
              curr_tm.tm_min, curr_tm.tm_sec);
    DBG("record to file:%s", filename);
    rf->file = fopen(filename, "wb");
}

void
stop_record_stream(uint32_t streamid) 
{
    INF("stop_record_stream:%u", streamid);
    rec_file * rf = get_rec_file(streamid);
    if(rf != NULL) {
        close_rec_file(rf);
    }
}
 
