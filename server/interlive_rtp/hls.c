/************************************************
 *  YueHonghui, 2013-07-25
 *  hhyue@tudou.com
 *  copyright:youku.com
 *  APPLE HLS buffer
 *  input: flv tags
 *  output: m3u8 list and ts
 *  ********************************************/

#include "hls.h"
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "stream_manager.h"
#include "util/flv.h"
#include "utils/memory.h"
#include "util/log.h"
#include "utils/buffer.hpp"
#include "util/flv2ts.h"

#define DURATION_OFFSET 5

#define M3U8_HEADER "#EXTM3U\n#EXT-X-ALLOW-CACHE:NO\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:%u\n#EXT-X-MEDIA-SEQUENCE:%lu\n"

#define M3U8_PERLINE "#EXTINF:%.1lf,\nt?b=%x&idx=%lu\n"

typedef struct segment
{
  buffer *ts;
  int64_t duration;
} segment;

struct hls_ctx
{
  uint32_t streamid;
  uint32_t target_duration_sec;
  uint32_t size;
  uint64_t head;
  uint64_t tail;
  uint32_t latest_timestamp;
  buffer *m3u8;
  buffer *tags;
  int32_t first_timestamp;
  segment segs[0];
};

hls_ctx *
hls_create(uint32_t streamid, uint32_t target_duration_sec, uint32_t cnt)
{
  size_t size = sizeof(hls_ctx)+cnt * sizeof(segment);
  uint32_t i = 0;
  hls_ctx *ctx = (hls_ctx *)mmalloc(size);

  if (NULL == ctx) {
    ERR("malloc failed. out of memory?");
    return NULL;
  }

  memset(ctx, 0, size);
  ctx->streamid = streamid;
  ctx->target_duration_sec = target_duration_sec;
  ctx->size = cnt;
  ctx->head = ctx->tail = 0;
  ctx->first_timestamp = -1;
  ctx->m3u8 = buffer_create_max(16 * 1024, 32 * 1024);

  //uint64_t len;
  //uint64_t media_sequence;
  //char tmp[1024];

  if (NULL == ctx->m3u8) {
    ERR("buffer create failed. out of memory?");
    goto failed;
  }
  //
  //memset(tmp, 0, sizeof(tmp));

  //media_sequence = ctx->tail - 1;
  //if (ctx->tail == 0)
  //{
  //    media_sequence = 0;
  //}

  //len = snprintf(tmp, sizeof(tmp)-1, M3U8_HEADER,
  //    ctx->target_duration_sec, media_sequence);

  //if (0 != buffer_expand_capacity(ctx->m3u8, len))
  //{
  //    ERR("buffer expand capacity failed.");
  //    goto failed;
  //}

  //buffer_append_ptr(ctx->m3u8, tmp, len);

  //memset(tmp, 0, sizeof(tmp));
  //len = snprintf(tmp, sizeof(tmp)-1, M3U8_PERLINE,
  //    ctx->target_duration_sec * 0.5, ctx->streamid, 0);

  //if (0 != buffer_expand_capacity(ctx->m3u8, len)) {
  //    ERR("buffer expand capacity failed.");
  //    buffer_reset(ctx->m3u8);
  //    goto failed;
  //}
  //buffer_append_ptr(ctx->m3u8, tmp, len);

  ctx->tags = buffer_create_max(128 * 1024, 8 * 1024 * 1024);
  if (NULL == ctx->tags) {
    ERR("buffer create failed. out of memory?");
    goto failed;
  }

  for (i = 0; i < cnt; i++) {
    ctx->segs[i].ts = buffer_create_max(128 * 1024, 8 * 1024 * 1024);
    if (NULL == ctx->segs[i].ts) {
      ERR("buffer create failed. out of memory?");
      goto failed;
    }
  }
  return ctx;

failed:
  if (ctx->m3u8)
    buffer_free(ctx->m3u8);
  if (ctx->tags)
    buffer_free(ctx->tags);
  for (i = 0; i < cnt; i++) {
    if (ctx->segs[i].ts) {
      buffer_free(ctx->segs[i].ts);
      ctx->segs[i].ts = NULL;
    }
  }
  mfree(ctx);
  ctx = NULL;
  return NULL;
}

void
hls_destroy(hls_ctx * ctx)
{
  assert(NULL != ctx);
  if (ctx->m3u8)
    buffer_free(ctx->m3u8);
  if (ctx->tags)
    buffer_free(ctx->tags);
  uint32_t i = 0;

  for (i = 0; i < ctx->size; i++) {
    if (ctx->segs[i].ts) {
      buffer_free(ctx->segs[i].ts);
      ctx->segs[i].ts = NULL;
    }
  }
  mfree(ctx);
}

int
hls_fetch_m3u8(hls_ctx * ctx, char **m3u8, size_t * m3u8_len)
{
  assert(NULL != m3u8 && NULL != m3u8_len);
  *m3u8 = (char *)buffer_data_ptr(ctx->m3u8);
  *m3u8_len = buffer_data_len(ctx->m3u8);
  return 0;
}

uint64_t get_latest_ts_idx(hls_ctx* ctx)
{
  return ctx->tail;
}

int
hls_fetch_ts(hls_ctx * ctx, uint64_t idx, char **ts, size_t * ts_len)
{
  assert(NULL != ctx);
  assert(NULL != ts);
  assert(NULL != ts_len);
  TRC("size = %u, head = %lu, tail = %lu, idx = %lu",
    ctx->size, ctx->head, ctx->tail, idx);
  if (ctx->head == ctx->tail)
    return -2;
  if (idx < ctx->head)
    return -1;
  if (idx >= ctx->tail)
    return 1;
  segment *sg = ctx->segs + idx % ctx->size;

  *ts = (char *)buffer_data_ptr(sg->ts);
  *ts_len = buffer_data_len(sg->ts);
  return 0;
}

#include "util/access.h"

static void
gen_m3u8(hls_ctx * ctx)
{
  if (ctx->head == ctx->tail)
    return;
  buffer_reset(ctx->m3u8);
  char tmp[1024];
  segment *seg = NULL;

  memset(tmp, 0, sizeof(tmp));
  uint64_t idx;
  if (ctx->tail < 3)
  {
    idx = 0;
  }
  else
  {
    idx = ctx->tail - 3;
  }

  uint64_t idx2 = idx;
  uint32_t target_duration = 1000;
  for (; idx2 < ctx->tail; idx2++)
  {
    seg = ctx->segs + idx2 % ctx->size;
    target_duration = MAX(target_duration, seg->duration + 500) / 1000 + 1;
  }

  int len = snprintf(tmp, sizeof(tmp)-1, M3U8_HEADER,
    ctx->target_duration_sec, idx);

  if (0 != buffer_expand_capacity(ctx->m3u8, len)) {
    ERR("buffer expand capacity failed.");
    return;
  }

  buffer_append_ptr(ctx->m3u8, tmp, len);

  for (; idx < ctx->tail; idx++) {
    seg = ctx->segs + idx % ctx->size;
    len =
      snprintf(tmp, sizeof(tmp)-1, M3U8_PERLINE,
      (seg->duration + 500) * 1.0 / 1000, ctx->streamid, idx);
    if (0 != buffer_expand_capacity(ctx->m3u8, len)) {
      ERR("buffer expand capacity failed.");
      buffer_reset(ctx->m3u8);
      return;
    }
    buffer_append_ptr(ctx->m3u8, tmp, len);
  }
}


#include <fstream>
using namespace std;

//static int32_t dump_ts(char* file_name, buffer* ts)
//{
//    ofstream file(file_name, ios::binary);
//    file.write(buffer_data_ptr(ts), buffer_data_len(ts));
//    file.close();
//    return buffer_data_len(ts);
//}


static int
convert_flv2ts(hls_ctx * ctx)
{
  const media_header *hdr = stream_manager_get_header(ctx->streamid, FLV_FLAG_BOTH);

  if (NULL == hdr) {
    WRN("get media header failed.");
    return -1;
  }

  int ret = -1;
  flv2ts_para para;

  memset(&para, 0, sizeof(para));
  para.flvdata = (uint8_t *)buffer_data_ptr(ctx->tags);
  para.flvdata_size = buffer_data_len(ctx->tags);
  if (hdr->payload_size < sizeof(flv_header)+2 * sizeof(flv_tag)) {
    ERR("invalid media header. payload_size = %u", hdr->payload_size);
    return -2;
  }
  uint32_t offset = sizeof(flv_header)+4;
  uint32_t total_sz = 0;

  while (offset < hdr->payload_size) {
    flv_tag *tag = (flv_tag *)(hdr->payload + offset);

    total_sz = flv_get_datasize(tag->datasize) + sizeof(flv_tag)+4;
    if (FLV_TAG_AUDIO == tag->type) {
      if (hdr->payload_size <
        sizeof(flv_tag_audio)+sizeof(flv_tag)+offset) {
        ERR("invalid flv audio tag. offset = %u", offset);
        return -1;
      }
      if (hdr->payload_size < sizeof(flv_tag_audio)+offset + 1) {
        offset += total_sz;
        continue;
      }
      if (flv_is_aac0(tag->data)) {
        DBG("flv aac0 find. size = %u", total_sz);
        para.aac_config = (uint8_t *)tag;
        para.aac_config_size = total_sz;
        offset += total_sz;
        continue;
      }
    }
    if (FLV_TAG_VIDEO == tag->type) {
      if (hdr->payload_size <
        sizeof(flv_tag_video)+sizeof(flv_tag)+offset) {
        ERR("invalid flv video tag. offset = %u", offset);
        return -1;
      }
      if (flv_is_avc0(tag->data)) {
        para.avc_config = (uint8_t *)tag;
        para.avc_config_size = total_sz;
        offset += total_sz;
        continue;
      }
    }
    offset += total_sz;
  }
  if (NULL == para.avc_config || NULL == para.aac_config
    || NULL == para.flvdata) {
    ERR("invalid flv2ts_para. avc_size = %zu, aac_size = %zu, data_size = %zu", para.avc_config_size, para.aac_config_size, para.flvdata_size);

    if (NULL == para.avc_config || NULL == para.aac_config)
    {
      return -2;
    }

    return -1;
  }
  if ((int64_t)ctx->tail - (int64_t)ctx->head < (int)ctx->size) {
    segment *s = ctx->segs + ctx->tail % ctx->size;

    assert(0 == buffer_data_len(s->ts));
    ret = flv2ts(&para, s->ts, &s->duration);
    if (0 != ret) {
      ERR("flv2ts failed. ret = %d", ret);
      buffer_reset(s->ts);
      s->duration = 0;
      return -1;
    }
    ctx->tail++;
    gen_m3u8(ctx);

    //char name[128];
    //sprintf(name, "v1_%d.ts", ctx->tail);
    //dump_ts(name, s->ts);
  }

  if (ctx->tail - ctx->head >= ctx->size) {
    segment *s = ctx->segs + ctx->head % ctx->size;

    buffer_reset(s->ts);
    s->duration = 0;
    ctx->head++;
  }
  return 0;
}

int
hls_input_flv_tags(hls_ctx * ctx, const uint8_t * tags, size_t tags_len)
{
  assert(NULL != ctx);
  assert(NULL != tags);
  size_t offset = 0;
  size_t total_sz = 0;
  flv_tag_video *vid = NULL;
  int32_t timestamp = -1;
  int32_t curr_duration = -1;
  //int rule1 = 0;
  int rule2 = 0, rule3 = 0;
  int ret = 0;

  while (offset < tags_len) {
    const flv_tag *tag = (const flv_tag *)(tags + offset);

    total_sz = sizeof(flv_tag)+flv_get_datasize(tag->datasize) + 4;
    if (FLV_TAG_AUDIO != tag->type && FLV_TAG_VIDEO != tag->type) {
      offset += total_sz;
      continue;
    }
    if ((FLV_TAG_AUDIO == tag->type && flv_is_aac0(tag->data))
      || (FLV_TAG_VIDEO == tag->type && flv_is_avc0(tag->data))) {
      offset += total_sz;
      continue;
    }
    timestamp = flv_get_timestamp(tag->timestamp);
    curr_duration = timestamp - ctx->first_timestamp;
    if (FLV_TAG_VIDEO == tag->type) {
      vid = (flv_tag_video *)tag->data;
      if (-1 == ctx->first_timestamp) {
        if (FLV_KEYFRAME != vid->video_info.frametype) {
          offset += total_sz;
          continue;
        }
        ctx->first_timestamp = timestamp;
        //push to cache
      }
      else {
        //rule1 = ( buffer_capacity(ctx->tags) <= total_sz );
        rule2 = (int)(curr_duration > (int)ctx->target_duration_sec * 1000);
        rule3 =
          (int)((curr_duration >
          (int)(ctx->target_duration_sec - DURATION_OFFSET) * 1000)
          && (FLV_KEYFRAME == vid->video_info.frametype));
        if (rule2 || rule3) {
          //push to convert
          ret = convert_flv2ts(ctx);

          if (0 != ret) {
            ERR("convert flv 2 ts failed.");
            return ret;
          }
          ctx->first_timestamp = -1;
          buffer_reset(ctx->tags);
          continue;
        }
        //push to cache
      }
    }
    if (FLV_TAG_AUDIO == tag->type) {
      if (-1 == ctx->first_timestamp) {
        offset += total_sz;
        continue;
      }
      else {
        //rule1 = (buffer_capacity(ctx->tags) <= total_sz);
        rule2 = (int)(curr_duration > (int)ctx->target_duration_sec * 1000);

        if (rule2) {
          //push to convert
          ret = convert_flv2ts(ctx);
          if (0 != ret)
          {
            ERR("convert flv 2 ts failed.");
            return ret;
          }
          ctx->first_timestamp = -1;
          buffer_reset(ctx->tags);
          continue;
        }
        //push to cache
      }
    }
    if (0 != buffer_expand_capacity(ctx->tags, total_sz)) {
      ERR("buffer_expand_capacity failed. ctx->tags(size = %zu, "
        "max = %zu, start = %zu, end = %zu), expand = %zu",
        ctx->tags->_size, ctx->tags->_max, ctx->tags->_start,
        ctx->tags->_end, total_sz);
      return -1;
    }
    ret = buffer_append_ptr(ctx->tags, tag, total_sz);
    assert(0 == ret);
    offset += total_sz;
  }
  return 0;
}
