#include "rtp2flv_remuxer.h"
#include "../avcodec/aac.h"
#include "../avformat/rtcp.h"
#include "../util/access.h"
#include "../target_config.h"
#include "../avformat/FLV.h"
#include "jitter_buffer.h"

#include <appframe/singleton.hpp>
#include "../rtp_trans/rtp_config.h"
#include "config.h"

using namespace media_manager;
using namespace avcodec;
using namespace avformat;
using namespace fragment;
using namespace std;
static uint8_t h264_split_code[] = { 0, 0, 0, 1 };

static config &_conf = *SINGLETON(config);

namespace media_manager {

  class Rtp2FlvTransformInfo {
  public:
    FLV *_flv;
    ::FLVHeader *_flv_header;
    media_manager::JitterBuffer *_audio_buffer;
    media_manager::JitterBuffer *_video_buffer;
    buffer *_video_frame_buffer;
    std::queue<int32_t> _video_frame_buffer_pos;
    uint32_t _video_frame_ts;
    uint32_t _muxer_queue_threshold;
    bool     _use_nalu_split_code;

    uint32_t _last_ts;
    uint16_t _last_video_seq;
    uint32_t _cur_nalu_size;

    StreamId_Ext _stream_id;

    uint32_t _audio_time_base;
    uint32_t _audio_channel;
    uint32_t _video_time_base;

    //rtp timestamp
    uint32_t _last_rtp_video_ts;
    uint32_t _last_rtp_audio_ts;

    //flv timestamp
    uint32_t _first_video_ts;
    uint32_t _first_audio_ts;
    uint32_t _video_offset;
    uint32_t _audio_offset;
    uint32_t _video_inc_req;
    uint32_t _audio_inc_req;
    uint32_t _out_sync_ts;

    bool  is_cur_frame_valied;

    bool is_sps_reach;
    bool is_pps_reach;
    bool is_adts_reach;
    bool is_audio_only;
    bool is_check_frame;
    buffer *_avc0_buffer;
    buffer *_aac0_buffer;
    //RTPMediaCache *_media_cache;

    std::deque<flv_tag *> _audio_out_queue;
    std::deque<flv_tag *> _video_out_queue;

  public:
    //media_manager::RTPMediaCache *get_media_cache();
    Rtp2FlvTransformInfo(StreamId_Ext stream_id);
    ~Rtp2FlvTransformInfo();
  };

  //RTPMediaCache *Rtp2FlvTransformInfo::get_media_cache() {
  //  if (_media_cache == NULL) {
  //    int status_code;
  //    _media_cache = CacheManager::get_rtp_cache_instance()->get_rtp_media_cache(_stream_id, status_code, true);
  //  }
  //  return _media_cache;
  //}

  Rtp2FlvTransformInfo::Rtp2FlvTransformInfo(StreamId_Ext stream_id) {
    TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");
    is_check_frame = common_config->enable_check_broken_frame;
    _use_nalu_split_code = common_config->enable_use_nalu_split_code;
    _audio_buffer = new media_manager::JitterBuffer();
    _audio_buffer->setBufferDuration(common_config->jitter_buffer_time);
    _video_buffer = new media_manager::JitterBuffer();
    _video_buffer->setBufferDuration(common_config->jitter_buffer_time);
    _stream_id = stream_id;
    _video_frame_buffer = buffer_init(1024 * 1024);
    _avc0_buffer = buffer_init(4096);
    _aac0_buffer = buffer_init(4096);
    _video_frame_ts = 0;
    _muxer_queue_threshold = common_config->muxer_queue_threshold;
    _flv_header = new ::FLVHeader();
    _flv = new FLV();
    is_sps_reach = false;
    is_pps_reach = false;
    is_adts_reach = false;
    is_audio_only = false;
    is_cur_frame_valied = true;
    _last_ts = 0;
    _first_video_ts = 0;
    _first_audio_ts = 0;
    _video_offset = 0;
    _audio_offset = 0;
    _video_inc_req = 0;
    _audio_inc_req = 0;
    _audio_channel = 0;
    //_media_cache = NULL;
    _last_video_seq = 0;
    _last_rtp_video_ts = 0;
    _last_rtp_audio_ts = 0;
    _out_sync_ts = 0;
  }

  Rtp2FlvTransformInfo::~Rtp2FlvTransformInfo() {
    if (_flv_header) {
      delete _flv_header;
    }
    if (_flv) {
      delete _flv;
    }
    if (_avc0_buffer) {
      buffer_free(_avc0_buffer);
    }
    if (_aac0_buffer) {
      buffer_free(_aac0_buffer);
    }
    if (_video_frame_buffer) {
      buffer_free(_video_frame_buffer);
    }
    if (_audio_buffer) {
      delete _audio_buffer;
    }
    if (_video_buffer) {
      delete _video_buffer;
    }

    while (!_audio_out_queue.empty()) {
      flv_tag *tag = _audio_out_queue.front();
      _audio_out_queue.pop_front();
      if (tag) {
        delete tag;
      }
    }
    while (!_video_out_queue.empty()) {
      flv_tag *tag = _video_out_queue.front();
      _video_out_queue.pop_front();
      if (tag) {
        delete tag;
      }
    }
    if (!_use_nalu_split_code) {
      while (!_video_frame_buffer_pos.empty()) {
        _video_frame_buffer_pos.pop();
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////

  RTP2FLVRemuxer *RTP2FLVRemuxer::_inst = NULL;

  RTP2FLVRemuxer::RTP2FLVRemuxer() {
    _uploader_cache_instance = FlvCacheManager::Instance();
  }

  RTP2FLVRemuxer * RTP2FLVRemuxer::get_instance() {
    if (_inst == NULL) {
      _inst = new RTP2FLVRemuxer;
    }
    return _inst;
  }

  RTP2FLVRemuxer::~RTP2FLVRemuxer() {
  }

  int32_t RTP2FLVRemuxer::set_rtp(const StreamId_Ext& stream_id,
    const avformat::RTP_FIXED_HEADER *pkt, uint16_t len, int32_t& status_code) {
    if (_conf.target_conf.enable_uploader) {
      Rtp2FlvTransformInfo *stream_meta = _get_stream_meta_by_sid(stream_id);
      if (!stream_meta) {
        DBG("invalied streamid %s,pkt skipped", stream_id.unparse().c_str());
        return -1;
      }

      if (pkt->payload == avformat::RTP_AV_H264) {
        JitterBuffer *video_buffer = stream_meta->_video_buffer;
        fragment::RTPBlock *rtpblock = new fragment::RTPBlock(pkt, len);
        video_buffer->SetRTPPacket(rtpblock);
        while (true) {
          fragment::RTPBlock *block = video_buffer->GetRTPPacket();
          if (block && block->is_valid()) {
            uint16_t pkt1_len;
            avformat::RTP_FIXED_HEADER* pkt1 = block->get(pkt1_len);
            _set_rtp_to_buffer(stream_meta, pkt1, pkt1_len, status_code);
            block->finalize();
            delete block;
          }
          else {
            break;
          }
        }
      }
      else if (pkt->payload == avformat::RTP_AV_AAC) {
        JitterBuffer *audio_buffer = stream_meta->_audio_buffer;
        fragment::RTPBlock *rtpblock = new fragment::RTPBlock(pkt, len);
        audio_buffer->SetRTPPacket(rtpblock);
        while (true) {
          fragment::RTPBlock *block = audio_buffer->GetRTPPacket();
          if (block && block->is_valid()) {
            uint16_t pkt1_len;
            avformat::RTP_FIXED_HEADER* pkt1 = block->get(pkt1_len);
            _set_rtp_to_buffer(stream_meta, pkt1, pkt1_len, status_code);
            block->finalize();
            delete block;
          }
          else {
            break;
          }
        }
      }

      if (!stream_meta->is_audio_only) {
        _set_flv_to_mm(stream_meta);
      }
    }

    return 0;

    //return RTPMediaManagerHelper::set_rtp(stream_id, pkt, len, status_code);
  }

  int32_t RTP2FLVRemuxer::set_sdp_char(const StreamId_Ext& stream_id, const char* sdp, int32_t len, int32_t& status_code) {
    Rtp2FlvTransformInfo *stream_meta = _get_stream_meta_by_sid(stream_id);
    if (!stream_meta) {
      WRN("invalied streamid %s,sdp skipped", stream_id.unparse().c_str());
      return -1;
    }

    SdpInfo sdpinfo;
    sdpinfo.parse_sdp_str(sdp, len);

    vector<rtp_media_info*> medias = sdpinfo.get_media_infos();
    vector<rtp_media_info*>::iterator media_it = medias.begin();

    for (media_it = medias.begin(); media_it != medias.end(); media_it++) {
      rtp_media_info*  media_info = *(media_it);
      if (media_info->payload_type == RTP_AV_H264) {
        if (media_info->extra_data && media_info->extra_data_len > 0)
        {
          uint8_t data[2048];
          const uint8_t *end = media_info->extra_data + media_info->extra_data_len;
          const uint8_t *r;

          r = ff_avc_find_startcode(media_info->extra_data, end);

          while (r && r < end)
          {
            const uint8_t *r1;
            while (!*(r++));
            r1 = ff_avc_find_startcode(r, end);
            int nalu_type = *(r)& 0x1f;
            if (nalu_type == NAL_SPS || nalu_type == NAL_PPS) {
              if (!r || !r1 || r1 > end || (r + (r - r1) > end))
              {
                WRN("incorrected avc config in sdp,skipped! streamid %s",
                  stream_id.unparse().c_str());
                break;
              }
              memcpy(data + sizeof(RTP_FIXED_HEADER), r, r - r1);
              set_rtp(stream_id, (RTP_FIXED_HEADER *)data, r - r1 +
                sizeof(RTP_FIXED_HEADER), status_code);
            }
            r = r1;
          }
        }
        stream_meta->_video_time_base = media_info->rate;
        stream_meta->_video_buffer->setTimeBase(stream_meta->_video_time_base);
      }
      else if (media_info->payload_type == RTP_AV_AAC) {
        if (medias.size() == 1) {
          stream_meta->is_audio_only = true;
        }
        stream_meta->_audio_time_base = media_info->rate;
        stream_meta->_audio_buffer->setTimeBase(stream_meta->_audio_time_base);
        stream_meta->_audio_channel = media_info->channels;
      }
    }
    stream_meta->_audio_buffer->_reset();
    stream_meta->_video_buffer->_reset();
    stream_meta->_first_audio_ts = 0;
    stream_meta->_first_video_ts = 0;
    stream_meta->_audio_offset = stream_meta->_video_offset = stream_meta->_out_sync_ts;
    buffer_reset(stream_meta->_video_frame_buffer);
    while (!stream_meta->_audio_out_queue.empty()) {
      flv_tag *tag = stream_meta->_audio_out_queue.front();
      stream_meta->_audio_out_queue.pop_front();
      if (tag) {
        delete tag;
      }
    }
    while (!stream_meta->_video_out_queue.empty()) {
      flv_tag *tag = stream_meta->_video_out_queue.front();
      stream_meta->_video_out_queue.pop_front();
      if (tag) {
        delete tag;
      }
    }
    if (!stream_meta->_use_nalu_split_code) {
      while (!stream_meta->_video_frame_buffer_pos.empty()) {
        stream_meta->_video_frame_buffer_pos.pop();
      }
    }

    return 0;
    //return RTPMediaManagerHelper::set_sdp_char(stream_id, sdp, len, status_code);
  }

  int32_t RTP2FLVRemuxer::_set_flv_to_mm(Rtp2FlvTransformInfo *stream_meta) {
    flv_tag *audio_tag = NULL;
    flv_tag *video_tag = NULL;

    if (!stream_meta->_audio_out_queue.empty()) {
      audio_tag = stream_meta->_audio_out_queue.front();
    }
    if (!stream_meta->_video_out_queue.empty()) {
      video_tag = stream_meta->_video_out_queue.front();
    }

    while (audio_tag && video_tag) {
      if (flv_get_timestamp(audio_tag->timestamp) != flv_get_timestamp(video_tag->timestamp)
        && (uint32_t)flv_get_timestamp(video_tag->timestamp) - (uint32_t)flv_get_timestamp(audio_tag->timestamp) < 0x80000000) {
        stream_meta->_audio_out_queue.pop_front();
        if (flv_get_timestamp(audio_tag->timestamp) >= stream_meta->_out_sync_ts) {
          _uploader_cache_instance->set_flv_tag(stream_meta->_stream_id, audio_tag);
          stream_meta->_out_sync_ts = flv_get_timestamp(audio_tag->timestamp);
        }
        delete audio_tag;
        audio_tag = NULL;
        if (!stream_meta->_audio_out_queue.empty())
        {
          audio_tag = stream_meta->_audio_out_queue.front();
        }
      }
      else {
        stream_meta->_video_out_queue.pop_front();
        if (flv_get_timestamp(video_tag->timestamp) >= stream_meta->_out_sync_ts
          && flv_get_datasize(video_tag->datasize) > 0) {
          _uploader_cache_instance->set_flv_tag(stream_meta->_stream_id, video_tag);
          stream_meta->_out_sync_ts = flv_get_timestamp(video_tag->timestamp);
        }
        delete video_tag;
        video_tag = NULL;
        if (!stream_meta->_video_out_queue.empty()) {
          video_tag = stream_meta->_video_out_queue.front();
        }
      }
    }

    if (stream_meta->_muxer_queue_threshold > 0) {
      while (stream_meta->_audio_out_queue.size() > 1
        && flv_get_timestamp(stream_meta->_audio_out_queue.back()->timestamp)
        - flv_get_timestamp(stream_meta->_audio_out_queue.front()->timestamp) > stream_meta->_muxer_queue_threshold
        ) {
        flv_tag *tag = stream_meta->_audio_out_queue.front();
        stream_meta->_audio_out_queue.pop_front();
        if (tag) {
          if (flv_get_timestamp(tag->timestamp) >= stream_meta->_out_sync_ts) {
            _uploader_cache_instance->set_flv_tag(stream_meta->_stream_id, tag);
            stream_meta->_out_sync_ts = flv_get_timestamp(tag->timestamp);
          }

          delete tag;
        }
        DBG("audio queue too large streamid %s _last_rtp_video_ts %u _last_rtp_audio_ts %u _first_video_ts %u _first_audio_ts %u _video_offset %u _audio_offset %u _out_sync_ts %u",
          stream_meta->_stream_id.unparse().c_str(), stream_meta->_last_rtp_video_ts, stream_meta->_last_rtp_audio_ts, stream_meta->_first_video_ts, stream_meta->_first_audio_ts,
          stream_meta->_video_offset, stream_meta->_audio_offset, stream_meta->_out_sync_ts);
      }

      while (stream_meta->_video_out_queue.size() > 1
        && flv_get_timestamp(stream_meta->_video_out_queue.back()->timestamp)
        - flv_get_timestamp(stream_meta->_video_out_queue.front()->timestamp) > stream_meta->_muxer_queue_threshold) {
        flv_tag *tag = stream_meta->_video_out_queue.front();
        stream_meta->_video_out_queue.pop_front();
        if (tag) {
          if (flv_get_timestamp(tag->timestamp) >= stream_meta->_out_sync_ts
            && flv_get_datasize(tag->datasize) > 0) {
            _uploader_cache_instance->set_flv_tag(stream_meta->_stream_id, tag);
            stream_meta->_out_sync_ts = flv_get_timestamp(tag->timestamp);
          }

          delete tag;
        }
        DBG("video queue too large streamid %s _last_rtp_video_ts %u _last_rtp_audio_ts %u _first_video_ts %u _first_audio_ts %u _video_offset %u _audio_offset %u _out_sync_ts %u",
          stream_meta->_stream_id.unparse().c_str(), stream_meta->_last_rtp_video_ts, stream_meta->_last_rtp_audio_ts, stream_meta->_first_video_ts, stream_meta->_first_audio_ts,
          stream_meta->_video_offset, stream_meta->_audio_offset, stream_meta->_out_sync_ts);
      }
    }
    return 0;
  }

  int32_t RTP2FLVRemuxer::_set_rtp_to_buffer(Rtp2FlvTransformInfo *stream_meta, const avformat::RTP_FIXED_HEADER* pkt, uint16_t len, int32_t& status) {
    buffer *video_frame_buffer = stream_meta->_video_frame_buffer;
    uint8_t *data = (uint8_t *)pkt;
    uint32_t pkt_ts = pkt->get_rtp_timestamp();
    uint32_t payload_offset = 0;

    if (pkt->extension) {
      EXTEND_HEADER *extend_header = (EXTEND_HEADER *)(data + sizeof(avformat::RTP_FIXED_HEADER));
      payload_offset += sizeof(EXTEND_HEADER);
      payload_offset += (ntohs(extend_header->rtp_extend_length) << 2);
    }

    if (pkt->payload == avformat::RTP_AV_H264) {
      FU_INDICATOR* fu_indicator = (FU_INDICATOR*)(pkt->data + payload_offset);
      uint8_t fu_type = fu_indicator->TYPE;

      if (stream_meta->_video_frame_ts != pkt_ts && buffer_data_len(video_frame_buffer) != 0) {
        _put_video_frame(stream_meta);
      }

      if (fu_type != 28) {
        if (!stream_meta->_use_nalu_split_code) {
          stream_meta->_video_frame_buffer_pos.push(buffer_data_len(video_frame_buffer));
        }
        //uint8_t frame_type = pkt->data[0];
        buffer_append_ptr(video_frame_buffer, h264_split_code, sizeof(h264_split_code));
        buffer_append_ptr(video_frame_buffer, pkt->data + payload_offset, len - sizeof(avformat::RTP_FIXED_HEADER) - payload_offset);
        stream_meta->_video_frame_ts = pkt_ts;
      }
      else {
        FU_HEADER* fu_header = (FU_HEADER*)(pkt->data + payload_offset + 1);
        if (stream_meta->_last_video_seq > 0) {
          stream_meta->is_cur_frame_valied = stream_meta->is_cur_frame_valied && pkt->get_seq() == stream_meta->_last_video_seq + 1;
          stream_meta->_last_video_seq = pkt->get_seq();
        }

        //  || !fu_indicator->F
        if (fu_header->S == 1)
        {
          if (!stream_meta->_use_nalu_split_code)
          {
            stream_meta->_video_frame_buffer_pos.push(buffer_data_len(video_frame_buffer));
          }

          stream_meta->_video_frame_ts = pkt_ts;
          buffer_append_ptr(video_frame_buffer, h264_split_code, sizeof(h264_split_code));

          NALU_HEADER nalu_header;
          nalu_header.F = fu_indicator->F;
          nalu_header.NRI = fu_indicator->NRI;
          nalu_header.TYPE = fu_header->TYPE;
          buffer_append_ptr(video_frame_buffer, &nalu_header, 1);
          stream_meta->_last_video_seq = pkt->get_seq();
        }
        buffer_append_ptr(video_frame_buffer, pkt->data + payload_offset + 2, len - sizeof(avformat::RTP_FIXED_HEADER) - payload_offset - 2);
      }

      if ((pkt->marker == 1) && buffer_data_len(video_frame_buffer) != 0) {
        _put_video_frame(stream_meta);
      }
    }
    else if (pkt->payload == avformat::RTP_AV_AAC) {
      int tag_len = 0;
      int status = 0;
      uint32_t payload_len = 0;
      uint32_t rtp_data_len = len - sizeof(avformat::RTP_FIXED_HEADER);

      while (payload_offset < rtp_data_len) {
        //adts
        if (*(pkt->data + payload_offset) == 0xff) {
          AACConfig config;
          config.parse_adts_header((uint8_t *)pkt->data + payload_offset, len - sizeof(avformat::RTP_FIXED_HEADER));
          stream_meta->_audio_time_base = config.get_samplingFrequency();
          stream_meta->_audio_buffer->setTimeBase(stream_meta->_audio_time_base);
          int frame_len = config.get_frame_length();
          if (!stream_meta->is_adts_reach)
          {
            //int audio_object = config.get_audioObjectType();
            //int channel = config.get_channelConfig();
            int config_len = 0;
            uint8_t * config_data = config.build_aac_specific(config_len);
            buffer_append_ptr(stream_meta->_aac0_buffer, config_data, config_len);
            stream_meta->is_adts_reach = true;
            _write_header(stream_meta);
          }

          payload_offset += ADTS_HEADER_SIZE;
          payload_len = frame_len - ADTS_HEADER_SIZE;
        }
        //latm
        else {
          //LATM_HEADER *latm_header = (LATM_HEADER *)pkt->data + payload_offset;
          if (!stream_meta->is_adts_reach) {
            AACConfig a_config;
            a_config.set_audioObjectType(AACPLUS_AOT_AAC_LC);
            a_config.set_channelConfig(stream_meta->_audio_channel);
            a_config.set_samplingFrequency(stream_meta->_audio_time_base);
            int config_len = 0;
            uint8_t * config_data = a_config.build_aac_specific(config_len);
            buffer_append_ptr(stream_meta->_aac0_buffer, config_data, config_len);
            stream_meta->is_adts_reach = true;
            _write_header(stream_meta);
          }
          payload_offset += sizeof(LATM_HEADER);
          payload_len = len - sizeof(avformat::RTP_FIXED_HEADER) - payload_offset;
        }

        if ((stream_meta->is_audio_only || (stream_meta->is_sps_reach && stream_meta->is_pps_reach)) && stream_meta->is_adts_reach) {
          if (stream_meta->_last_rtp_audio_ts > pkt_ts) {
            stream_meta->_audio_offset +=
              (uint32_t)(stream_meta->_last_rtp_audio_ts / ((float)stream_meta->_audio_time_base / 1000.0))
              + (uint32_t)((0xFFFFFFFF - stream_meta->_last_rtp_audio_ts) / ((float)stream_meta->_audio_time_base / 1000.0));
            INF("streamid %s audio rtptime maxvalue reached process _audio_offset %u %u ts %u seq %d",
              stream_meta->_stream_id.unparse().c_str(), stream_meta->_audio_offset
              , stream_meta->_last_rtp_audio_ts, pkt_ts, (int)pkt->get_seq());
          }

          stream_meta->_last_rtp_audio_ts = pkt_ts;

          int32_t flv_ts = (uint32_t)(pkt_ts / ((float)stream_meta->_audio_time_base / 1000.0)) & 0x7FFFFFFFF;

          flv_tag* tag = stream_meta->_flv->generate_audio_tag(pkt->data + payload_offset,
            payload_len, flv_ts, tag_len, status);

          if (stream_meta->_first_audio_ts == 0) {
            stream_meta->_first_audio_ts = (uint32_t)FLV::flv_get_timestamp(tag->timestamp);
          }

          FLV::flv_set_timestamp(tag->timestamp, (uint32_t)FLV::flv_get_timestamp(tag->timestamp)
            - stream_meta->_first_audio_ts + stream_meta->_audio_offset);

          if (stream_meta->_first_video_ts == 0) {
            stream_meta->_video_offset = (uint32_t)FLV::flv_get_timestamp(tag->timestamp);
          }

          if (stream_meta->is_audio_only) {
            int ret = _uploader_cache_instance->set_flv_tag(stream_meta->_stream_id, tag);
            if (ret < 0) {
              ERR("set audio flv tag error ret %d", ret);
            }
          }
          else {
            flv_tag *queued_tag = (flv_tag *)new uint8_t[tag_len];
            memcpy(queued_tag, tag, tag_len);
            stream_meta->_audio_out_queue.push_back(queued_tag);
          }
        }
        payload_offset += payload_len;
      }

    }
    return 0;
  }

  void RTP2FLVRemuxer::_write_header(Rtp2FlvTransformInfo *stream_meta) {
    if ((stream_meta->is_audio_only || (stream_meta->is_sps_reach && stream_meta->is_pps_reach)) && stream_meta->is_adts_reach)
    {
      ::FLVHeader* flv_hdr = stream_meta->_flv_header;
      int32_t status = 0;
      flv_hdr->build((const uint8_t *)buffer_data_ptr(stream_meta->_avc0_buffer), buffer_data_len(stream_meta->_avc0_buffer),
        (const uint8_t *)buffer_data_ptr(stream_meta->_aac0_buffer), buffer_data_len(stream_meta->_aac0_buffer), status);
      int32_t len = 0;
      flv_header* header = flv_hdr->get_header(len);
      //_uploader_cache_instance->init_stream(stream_meta->_stream_id);
      _uploader_cache_instance->set_flv_header(stream_meta->_stream_id, header, len);
    }
  }

  void RTP2FLVRemuxer::_put_video_frame(Rtp2FlvTransformInfo *stream_meta) {
    buffer * video_frame_buffer = stream_meta->_video_frame_buffer;
    const uint8_t *payload = (uint8_t *)buffer_data_ptr(video_frame_buffer);
    int payload_len = buffer_data_len(stream_meta->_video_frame_buffer);

    //pre-process nalu-size;
    if (!stream_meta->_use_nalu_split_code) {
      int32_t lastpos = -1;
      uint32_t pos = 0;
      while (!stream_meta->_video_frame_buffer_pos.empty()) {
        //int size = stream_meta->_video_frame_buffer_pos.size();
        pos = stream_meta->_video_frame_buffer_pos.front();
        stream_meta->_video_frame_buffer_pos.pop();
        if (lastpos >= 0) {
          *((uint32_t *)(buffer_data_ptr(video_frame_buffer) + lastpos)) = htonl(pos - lastpos - 4);
        }
        lastpos = pos;
      }

      if (lastpos > 0) {
        *((uint32_t *)(buffer_data_ptr(video_frame_buffer) + lastpos)) = htonl(payload_len - lastpos - 4);
      }
    }

    if (!stream_meta->is_pps_reach || !stream_meta->is_sps_reach) {
      const uint8_t *payload = (uint8_t *)buffer_data_ptr(video_frame_buffer);
      int payload_len = buffer_data_len(video_frame_buffer);
      const uint8_t *end = payload + payload_len;
      const uint8_t *r = payload;

      if (*(r) == 0 && *(r + 1) == 0 && *(r + 2) == 0 && *(r + 3) == 1) {
        r = ff_avc_find_startcode(payload, end);
        while (r < end) {
          const uint8_t *r1;
          while (!*(r++));
          r1 = ff_avc_find_startcode(r, end);
          int nalu_type = *(r);
          if (((nalu_type & 0x1f) == 0x07) && ((nalu_type & 0x60) != 0) && !stream_meta->is_sps_reach) {
            buffer_append_ptr(stream_meta->_avc0_buffer, h264_split_code, sizeof(h264_split_code));
            buffer_append_ptr(stream_meta->_avc0_buffer, r, r1 - r);
            stream_meta->is_sps_reach = true;
          }
          else if (((nalu_type & 0x1f) == 0x08) && ((nalu_type & 0x60) != 0) && !stream_meta->is_pps_reach) {
            buffer_append_ptr(stream_meta->_avc0_buffer, h264_split_code, sizeof(h264_split_code));
            buffer_append_ptr(stream_meta->_avc0_buffer, r, r1 - r);
            stream_meta->is_pps_reach = true;
          }

          //if (*(r) == 0x08) {
          //    WRN("recv invalied pps by splitcode  streamid  %s len %d", stream_meta->_stream_id.c_str(), payload_len);
          //}

          r = r1;

          if (stream_meta->is_sps_reach && stream_meta->is_pps_reach) {
            break;
          }
        }
      }
      else {
        r = payload;
        while (r < end) {
          uint32_t nalu_size = ntohl(*((uint32_t *)r));
          if (r + nalu_size > end) {
            DBG("nalu size error %u,skip", nalu_size);
            break;
          }
          uint8_t nalu_type = *(r + 4);
          r += 4;
          if (((nalu_type & 0x1f) == 0x07) && ((nalu_type & 0x60) != 0) && !stream_meta->is_sps_reach) {
            buffer_append_ptr(stream_meta->_avc0_buffer, h264_split_code, sizeof(h264_split_code));
            buffer_append_ptr(stream_meta->_avc0_buffer, r, nalu_size);
            stream_meta->is_sps_reach = true;
          }
          else if (((nalu_type & 0x1f) == 0x08) && ((nalu_type & 0x60) != 0) && !stream_meta->is_pps_reach) {
            buffer_append_ptr(stream_meta->_avc0_buffer, h264_split_code, sizeof(h264_split_code));
            buffer_append_ptr(stream_meta->_avc0_buffer, r, nalu_size);
            stream_meta->is_pps_reach = true;
          }
          r += nalu_size;

          if (*(r + 4) == 0x08) {
            WRN("recv invalied pps by size num  streamid  %s len %d", stream_meta->_stream_id.c_str(), payload_len);
          }

          if (stream_meta->is_sps_reach && stream_meta->is_pps_reach) {
            break;
          }
        }
      }
      _write_header(stream_meta);
    }

    if (stream_meta->is_sps_reach && stream_meta->is_pps_reach && stream_meta->is_adts_reach) {
      int tag_len = 0;
      int status = 0;

      if (stream_meta->_last_rtp_video_ts > stream_meta->_video_frame_ts) {
        stream_meta->_video_offset +=
          (uint32_t)((stream_meta->_last_rtp_video_ts) / ((float)stream_meta->_video_time_base / 1000.0))
          + (uint32_t)((0xFFFFFFFF - stream_meta->_last_rtp_video_ts) / ((float)stream_meta->_video_time_base / 1000.0));
        INF("streamid %s video rtptime maxvalue reached process _video_offset %u %u %u ",
          stream_meta->_stream_id.unparse().c_str(), stream_meta->_video_offset,
          stream_meta->_last_rtp_video_ts, stream_meta->_video_frame_ts);
      }

      stream_meta->_last_rtp_video_ts = stream_meta->_video_frame_ts;

      int32_t flv_ts = (uint32_t)(stream_meta->_video_frame_ts / ((float)stream_meta->_video_time_base / 1000.0)) & 0x7FFFFFFFF;


      flv_tag* tag = stream_meta->_flv->generate_video_tag(payload, payload_len,
        flv_ts, tag_len, status);

      if (stream_meta->_first_video_ts == 0) {
        stream_meta->_first_video_ts = (uint32_t)FLV::flv_get_timestamp(tag->timestamp);
      }

      FLV::flv_set_timestamp(tag->timestamp, (uint32_t)FLV::flv_get_timestamp(tag->timestamp)
        - stream_meta->_first_video_ts + stream_meta->_video_offset);
      if (stream_meta->_first_audio_ts == 0) {
        stream_meta->_audio_offset = FLV::flv_get_timestamp(tag->timestamp);
      }

      if ((stream_meta->is_check_frame && !stream_meta->is_cur_frame_valied) || ((tag->data[0] & 0x0f) != 7)) {
        //invalied tag
        flv_set_datasize(tag->datasize, 0);
      }

      flv_tag *queued_tag = (flv_tag *)new uint8_t[tag_len];
      memcpy(queued_tag, tag, tag_len);
      stream_meta->_video_out_queue.push_back(queued_tag);
    }
    buffer_reset(video_frame_buffer);
    stream_meta->is_cur_frame_valied = true;
    stream_meta->_last_video_seq = 0;
    if (!stream_meta->_use_nalu_split_code) {
      while (!stream_meta->_video_frame_buffer_pos.empty()) {
        stream_meta->_video_frame_buffer_pos.pop();
      }
    }
  }

  Rtp2FlvTransformInfo * RTP2FLVRemuxer::_get_stream_meta_by_sid(const StreamId_Ext& stream_id) {
    Rtp2FlvTransformInfo *result = NULL;
    auto it = _meta_map.find(stream_id);
    if (it != _meta_map.end()) {
      result = it->second;
    }
    else {
      result = _meta_map[stream_id] = new Rtp2FlvTransformInfo(stream_id);
      INF("recv stream %s pkt, start rtp2flv remuxer", stream_id.unparse().c_str());
    }
    return result;
  }

}
