﻿#ifndef _FLV2RTP_REMUXER_H_
#define _FLV2RTP_REMUXER_H_

#include "cache_manager.h"
#include "../avformat/rtp.h"
//#include "../rtp_trans/rtp_media_manager_helper.h"

namespace media_manager {

  class Rtp2FlvTransformInfo;

  // 这个类将rtp转换成flv
  // 对于udp的rtp，其核心应该有jitterbuffer + 丢包防马赛克。
  // 不幸的是代码中使用的jitterbuffer很烂，也没有做丢包防马赛克处理。
  class RTP2FLVRemuxer {
  public:
    static RTP2FLVRemuxer * get_instance();
    ~RTP2FLVRemuxer();

    int32_t set_rtp(const StreamId_Ext& stream_id, const avformat::RTP_FIXED_HEADER *rtp, uint16_t len, int32_t& status_code);
    int32_t set_sdp_char(const StreamId_Ext& stream_id, const char* sdp, int32_t len, int32_t& status_code);

    void DestroyStream(const StreamId_Ext& streamid);

  protected:
    RTP2FLVRemuxer();
    int32_t _set_rtp_to_buffer(Rtp2FlvTransformInfo *stream_meta, const avformat::RTP_FIXED_HEADER* rtp, uint16_t len, int32_t& status);
    int32_t _set_flv_to_mm(Rtp2FlvTransformInfo *stream_meta);
    void _write_header(Rtp2FlvTransformInfo *stream_meta);
    void _put_video_frame(Rtp2FlvTransformInfo *stream_meta);
    Rtp2FlvTransformInfo * _get_stream_meta_by_sid(const StreamId_Ext& stream_id);

  private:
    __gnu_cxx::hash_map<StreamId_Ext, Rtp2FlvTransformInfo*> _meta_map;
    FlvCacheManager* _uploader_cache_instance;
    static RTP2FLVRemuxer *_inst;
  };

}
#endif
