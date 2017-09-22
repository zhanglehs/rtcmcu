/************************************************
 *  YueHonghui, 2013-07-25
 *  hhyue@tudou.com
 *  copyright:youku.com
 *  APPLE HLS buffer
 *  input: flv tags
 *  output: m3u8 list and ts
 *  ********************************************/

#ifndef HLS_H_
#define HLS_H_
#include <stdint.h>
#include <unistd.h>

#if (defined __cplusplus && !defined _WIN32)
extern "C" 
{
#endif

typedef struct hls_ctx hls_ctx;

hls_ctx *hls_create(uint32_t streamid, uint32_t segment_sec,
                    uint32_t segment_cnt);
void hls_destroy(hls_ctx * ctx);
int hls_fetch_m3u8(hls_ctx * ctx, char **m3u8, size_t * len);
uint64_t get_latest_ts_idx(hls_ctx* ctx);
int hls_fetch_ts(hls_ctx * ctx, uint64_t index, char **ts, size_t * ts_len);
int hls_input_flv_tags(hls_ctx * ctx, const uint8_t * tags, size_t tags_len);

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
