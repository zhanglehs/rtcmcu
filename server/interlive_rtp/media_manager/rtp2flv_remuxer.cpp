#include "rtp2flv_remuxer.h"
#include "../avcodec/aac.h"
#include "../avformat/rtcp.h"
#include "../util/access.h"
#include "../target_config.h"

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
  RTP2FLVRemuxer *RTP2FLVRemuxer::_inst = NULL;

  RTP2FLVRemuxer::RTP2FLVRemuxer() {
    SINGLETON(WhitelistManager)->add_observer(this, WHITE_LIST_EV_STOP);
    _uploader_cache_instance = CacheManager::get_uploader_cache_instance();
  }

  RTP2FLVRemuxer::~RTP2FLVRemuxer() {
  }

  int32_t RTP2FLVRemuxer::set_rtp(const StreamId_Ext& stream_id,
    const avformat::RTP_FIXED_HEADER *pkt, uint16_t len, int32_t& status_code) {
    if (_conf.target_conf.enable_uploader) {

    StreamMeta *stream_meta = _get_stream_meta_by_sid(stream_id);

    if (!stream_meta)
    {
      DBG("invalied streamid %s,pkt skipped", stream_id.unparse().c_str());
      return -1;
    }

    if (pkt->payload == avformat::RTP_AV_H264)
    {
      JitterBuffer *video_buffer = stream_meta->_video_buffer;
      fragment::RTPBlock *rtpblock = new fragment::RTPBlock(pkt, len);
      video_buffer->SetRTPPacket(rtpblock);
      while (true)
      {
        fragment::RTPBlock *block = video_buffer->GetRTPPacket();
        if (block && block->is_valid())
        {
          uint16_t pkt1_len;
          avformat::RTP_FIXED_HEADER* pkt1 = block->get(pkt1_len);

          _set_rtp_to_buffer(stream_meta, pkt1, pkt1_len, status_code);
          //if (RTPTransConfig::log_level >= RTP_TRACE_LEVEL_RTCP)
          //{
          //  DBG("RTP_J_NORMAL streamid %s ssrc %u seq %d payload %d timestamp %u len %d",stream_meta->_stream_id.unparse().c_str(),pkt1->get_ssrc(),pkt1->get_seq(),pkt1->payload,pkt1->get_rtp_timestamp(),pkt1_len);
          //}
          block->finalize();
          delete block;
        }
        else {
          break;
        }
      }
    }
    else if (pkt->payload == avformat::RTP_AV_AAC || pkt->payload == avformat::RTP_AV_AAC_GXH || pkt->payload == avformat::RTP_AV_AAC_MAIN)
    {
      JitterBuffer *audio_buffer = stream_meta->_audio_buffer;
      fragment::RTPBlock *rtpblock = new fragment::RTPBlock(pkt, len);
      audio_buffer->SetRTPPacket(rtpblock);
      while (true)
      {
        fragment::RTPBlock *block = audio_buffer->GetRTPPacket();
        if (block && block->is_valid())
        {
          uint16_t pkt1_len;
          avformat::RTP_FIXED_HEADER* pkt1 = block->get(pkt1_len);
          _set_rtp_to_buffer(stream_meta, pkt1, pkt1_len, status_code);
          //if (RTPTransConfig::log_level >= RTP_TRACE_LEVEL_RTCP)
          //{
          //   DBG("RTP_J_NORMAL streamid %s ssrc %u seq %d payload %d timestamp %u len %d",stream_meta->_stream_id.unparse().c_str(),pkt1->get_ssrc(),pkt1->get_seq(),pkt1->payload,pkt1->get_rtp_timestamp(),pkt1_len);
          //}
          block->finalize();
          delete block;
        }
        else {
          break;
        }
      }
    }


    if (!stream_meta->is_audio_only)
    {
      _set_flv_to_mm(stream_meta);
    }

    }

    return RTPMediaManagerHelper::set_rtp(stream_id, pkt, len, status_code);
  }

  const avformat::RTP_FIXED_HEADER* RTP2FLVRemuxer::get_rtp_by_ssrc_seq(const StreamId_Ext& stream_id, uint32_t ssrc,
    uint16_t seq, uint16_t &len, int32_t& status_code, bool return_next_valid_packet) {
    return RTPMediaManagerHelper::get_rtp_by_ssrc_seq(stream_id, ssrc, seq, len, status_code, return_next_valid_packet);
  }

  const avformat::RTP_FIXED_HEADER* RTP2FLVRemuxer::get_rtp_by_mediatype_seq(const StreamId_Ext& stream_id, avformat::RTPMediaType type,
    uint16_t seq, uint16_t &len, int32_t& status_code, bool return_next_valid_packet) {
    return RTPMediaManagerHelper::get_rtp_by_mediatype_seq(stream_id, type, seq, len, status_code, return_next_valid_packet);
  }

  int32_t RTP2FLVRemuxer::set_sdp_char(const StreamId_Ext& stream_id, const char* sdp, int32_t len, int32_t& status_code) {
    StreamMeta *stream_meta = _get_stream_meta_by_sid(stream_id);

    if (!stream_meta)
    {
      WRN("invalied streamid %s,sdp skipped", stream_id.unparse().c_str());
      return -1;
    }

    SdpInfo sdpinfo;
    sdpinfo.parse_sdp_str(sdp, len);

    vector<rtp_media_info*> medias = sdpinfo.get_media_infos();
    vector<rtp_media_info*>::iterator media_it = medias.begin();

    for (media_it = medias.begin(); media_it != medias.end(); media_it++)
    {
      rtp_media_info*  media_info = *(media_it);
      if (media_info->payload_type == RTP_AV_H264)
      {
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
      else if (media_info->payload_type == RTP_AV_AAC || media_info->payload_type == avformat::RTP_AV_AAC_GXH || media_info->payload_type == avformat::RTP_AV_AAC_MAIN)
      {
        if (medias.size() == 1)
        {
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
    while (!stream_meta->_audio_out_queue.empty())
    {
      flv_tag *tag = stream_meta->_audio_out_queue.front();
      stream_meta->_audio_out_queue.pop_front();
      if (tag)
      {
        delete tag;
      }
    }
    while (!stream_meta->_video_out_queue.empty())
    {
      flv_tag *tag = stream_meta->_video_out_queue.front();
      stream_meta->_video_out_queue.pop_front();
      if (tag)
      {
        delete tag;
      }
    }
    if (!stream_meta->_use_nalu_split_code)
    {
      while (!stream_meta->_video_frame_buffer_pos.empty())
      {
        stream_meta->_video_frame_buffer_pos.pop();
      }
    }
    return RTPMediaManagerHelper::set_sdp_char(stream_id, sdp, len, status_code);
  }

  const char* RTP2FLVRemuxer::get_sdp_char(const StreamId_Ext& stream_id, int32_t& len, int32_t& status_code) {
    StreamMeta *stream_meta = _get_stream_meta_by_sid(stream_id);

    if (!stream_meta)
    {
      WRN("invalied streamid %s,sdp skipped", stream_id.unparse().c_str());
      return NULL;
    }

    RTPMediaCache *media_cache = stream_meta->get_media_cache();
    if (media_cache == NULL) {
      return NULL;
    }
    SdpInfo* sdp = media_cache->get_sdp();

    if (sdp == NULL)
    {
      return NULL;
    }

    const char* sdp_char = sdp->get_sdp_char();
    if (sdp_char == NULL)
    {
      return NULL;
    }

    len = strlen(sdp_char);
    return sdp_char;
  }

  int32_t RTP2FLVRemuxer::set_sdp_str(const StreamId_Ext& stream_id, const std::string& sdp, int32_t& status_code) {
    return RTP2FLVRemuxer::set_sdp_char(stream_id, (char *)sdp.c_str(), sdp.length(), status_code);
  }
  std::string RTP2FLVRemuxer::get_sdp_str(const StreamId_Ext& stream_id, int32_t& status_code) {
    int32_t len = 0;
    const char *sdp = get_sdp_char(stream_id, len, status_code);
    if (sdp) {
      return sdp;
    }
    else {
      return "";
    }
    //auto it = _meta_map.find(stream_id);
    //if (it == _meta_map.end()) {
    //  return "";
    //}
    //StreamMeta *stream_meta = it->second;
    //RTPMediaCache *media_cache = stream_meta->get_media_cache();
    //SdpInfo* sdp = media_cache->get_sdp();

    //if (sdp == NULL)
    //{
    //  return string("");
    //}

    //return sdp->get_sdp_str();
  }

  int32_t RTP2FLVRemuxer::_set_flv_to_mm(StreamMeta *stream_meta) {
    flv_tag *audio_tag = NULL;
    flv_tag *video_tag = NULL;

    if (!stream_meta->_audio_out_queue.empty())
    {
      audio_tag = stream_meta->_audio_out_queue.front();
    }

    if (!stream_meta->_video_out_queue.empty())
    {
      video_tag = stream_meta->_video_out_queue.front();
    }


    while (audio_tag && video_tag)
    {
      if (flv_get_timestamp(audio_tag->timestamp) != flv_get_timestamp(video_tag->timestamp)
        && (uint32_t)flv_get_timestamp(video_tag->timestamp) - (uint32_t)flv_get_timestamp(audio_tag->timestamp) < 0x80000000)
      {
        stream_meta->_audio_out_queue.pop_front();
        if (flv_get_timestamp(audio_tag->timestamp) >= stream_meta->_out_sync_ts)
        {
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
          && flv_get_datasize(video_tag->datasize) > 0)
        {
          _uploader_cache_instance->set_flv_tag(stream_meta->_stream_id, video_tag);
          stream_meta->_out_sync_ts = flv_get_timestamp(video_tag->timestamp);
        }
        delete video_tag;
        video_tag = NULL;
        if (!stream_meta->_video_out_queue.empty())
        {
          video_tag = stream_meta->_video_out_queue.front();
        }
      }
    }

    if (stream_meta->_muxer_queue_threshold > 0)
    {
      while (stream_meta->_audio_out_queue.size() > 1
        && flv_get_timestamp(stream_meta->_audio_out_queue.back()->timestamp)
        - flv_get_timestamp(stream_meta->_audio_out_queue.front()->timestamp) > stream_meta->_muxer_queue_threshold
        )
      {
        flv_tag *tag = stream_meta->_audio_out_queue.front();
        stream_meta->_audio_out_queue.pop_front();
        if (tag)
        {
          if (flv_get_timestamp(tag->timestamp) >= stream_meta->_out_sync_ts)
          {
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
        - flv_get_timestamp(stream_meta->_video_out_queue.front()->timestamp) > stream_meta->_muxer_queue_threshold)
      {
        flv_tag *tag = stream_meta->_video_out_queue.front();
        stream_meta->_video_out_queue.pop_front();
        if (tag)
        {
          if (flv_get_timestamp(tag->timestamp) >= stream_meta->_out_sync_ts
            && flv_get_datasize(tag->datasize) > 0)
          {
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

  int32_t RTP2FLVRemuxer::_set_rtp_to_buffer(StreamMeta *stream_meta, const avformat::RTP_FIXED_HEADER* pkt, uint16_t len, int32_t& status) {
    buffer *video_frame_buffer = stream_meta->_video_frame_buffer;
    uint8_t *data = (uint8_t *)pkt;
    uint32_t pkt_ts = pkt->get_rtp_timestamp();
    uint32_t payload_offset = 0;

    if (pkt->extension)
    {
      EXTEND_HEADER *extend_header = (EXTEND_HEADER *)(data + sizeof(avformat::RTP_FIXED_HEADER));
      payload_offset += sizeof(EXTEND_HEADER);
      payload_offset += (ntohs(extend_header->rtp_extend_length) << 2);
    }

    if (pkt->payload == avformat::RTP_AV_H264)
    {

      FU_INDICATOR* fu_indicator = (FU_INDICATOR*)(pkt->data + payload_offset);
      uint8_t fu_type = fu_indicator->TYPE;

      if (stream_meta->_video_frame_ts != pkt_ts && buffer_data_len(video_frame_buffer) != 0)
      {
        _put_video_frame(stream_meta);
      }


      if (fu_type != 28)
      {
        if (!stream_meta->_use_nalu_split_code)
        {
          stream_meta->_video_frame_buffer_pos.push(buffer_data_len(video_frame_buffer));
        }
        //uint8_t frame_type = pkt->data[0];
        buffer_append_ptr(video_frame_buffer, h264_split_code, sizeof(h264_split_code));
        buffer_append_ptr(video_frame_buffer, pkt->data + payload_offset, len - sizeof(avformat::RTP_FIXED_HEADER) - payload_offset);
        stream_meta->_video_frame_ts = pkt_ts;
      }
      else
      {
        FU_HEADER* fu_header = (FU_HEADER*)(pkt->data + payload_offset + 1);


        if (stream_meta->_last_video_seq > 0)
        {
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

      if ((pkt->marker == 1) && buffer_data_len(video_frame_buffer) != 0)
      {
        _put_video_frame(stream_meta);
      }

    }
    else if (pkt->payload == avformat::RTP_AV_AAC || pkt->payload == avformat::RTP_AV_AAC_GXH || pkt->payload == avformat::RTP_AV_AAC_MAIN)
    {
      int tag_len = 0;
      int status = 0;
      uint32_t payload_len = 0;
      uint32_t rtp_data_len = len - sizeof(avformat::RTP_FIXED_HEADER);

      while (payload_offset < rtp_data_len) {
        //adts
        if (*(pkt->data + payload_offset) == 0xff)
        {
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
          if (!stream_meta->is_adts_reach)
          {
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


        if ((stream_meta->is_audio_only || (stream_meta->is_sps_reach && stream_meta->is_pps_reach)) && stream_meta->is_adts_reach)
        {

          if (stream_meta->_last_rtp_audio_ts > pkt_ts)
          {
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

          if (stream_meta->_first_audio_ts == 0)
          {
            stream_meta->_first_audio_ts = (uint32_t)FLV::flv_get_timestamp(tag->timestamp);
          }

          FLV::flv_set_timestamp(tag->timestamp, (uint32_t)FLV::flv_get_timestamp(tag->timestamp)
            - stream_meta->_first_audio_ts + stream_meta->_audio_offset);

          if (stream_meta->_first_video_ts == 0) {
            stream_meta->_video_offset = (uint32_t)FLV::flv_get_timestamp(tag->timestamp);
          }

          if (stream_meta->is_audio_only)
          {
            int ret = _uploader_cache_instance->set_flv_tag(stream_meta->_stream_id, tag);
            if (ret < 0)
            {
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

  void RTP2FLVRemuxer::_write_header(StreamMeta *stream_meta) {
    if ((stream_meta->is_audio_only || (stream_meta->is_sps_reach && stream_meta->is_pps_reach)) && stream_meta->is_adts_reach)
    {
      ::FLVHeader* flv_hdr = stream_meta->_flv_header;
      int32_t status = 0;
      flv_hdr->build((const uint8_t *)buffer_data_ptr(stream_meta->_avc0_buffer), buffer_data_len(stream_meta->_avc0_buffer),
        (const uint8_t *)buffer_data_ptr(stream_meta->_aac0_buffer), buffer_data_len(stream_meta->_aac0_buffer), status);
      int32_t len = 0;
      flv_header* header = flv_hdr->get_header(len);
      UploaderCacheManagerInterface* uploader_cache_instance = CacheManager::get_uploader_cache_instance();
      uploader_cache_instance->init_stream(stream_meta->_stream_id);
      uploader_cache_instance->set_flv_header(stream_meta->_stream_id, header, len);
    }
  }

  void RTP2FLVRemuxer::_put_video_frame(StreamMeta *stream_meta) {
    buffer * video_frame_buffer = stream_meta->_video_frame_buffer;
    const uint8_t *payload = (uint8_t *)buffer_data_ptr(video_frame_buffer);
    int payload_len = buffer_data_len(stream_meta->_video_frame_buffer);

    //pre-process nalu-size;
    if (!stream_meta->_use_nalu_split_code)
    {
      int32_t lastpos = -1;
      uint32_t pos = 0;
      while (!stream_meta->_video_frame_buffer_pos.empty())
      {
        //int size = stream_meta->_video_frame_buffer_pos.size();
        pos = stream_meta->_video_frame_buffer_pos.front();
        stream_meta->_video_frame_buffer_pos.pop();
        if (lastpos >= 0)
        {
          *((uint32_t *)(buffer_data_ptr(video_frame_buffer) + lastpos)) = htonl(pos - lastpos - 4);
        }
        lastpos = pos;
      }

      if (lastpos > 0)
      {
        *((uint32_t *)(buffer_data_ptr(video_frame_buffer) + lastpos)) = htonl(payload_len - lastpos - 4);
      }
    }

    if (!stream_meta->is_pps_reach || !stream_meta->is_sps_reach)
    {
      const uint8_t *payload = (uint8_t *)buffer_data_ptr(video_frame_buffer);
      int payload_len = buffer_data_len(video_frame_buffer);
      const uint8_t *end = payload + payload_len;
      const uint8_t *r = payload;


      if (*(r) == 0 && *(r + 1) == 0 && *(r + 2) == 0 && *(r + 3) == 1)
      {
        r = ff_avc_find_startcode(payload, end);
        while (r < end)
        {
          const uint8_t *r1;
          while (!*(r++));
          r1 = ff_avc_find_startcode(r, end);
          int nalu_type = *(r);
          if (((nalu_type & 0x1f) == 0x07) && ((nalu_type & 0x60) != 0) && !stream_meta->is_sps_reach)
          {
            buffer_append_ptr(stream_meta->_avc0_buffer, h264_split_code, sizeof(h264_split_code));
            buffer_append_ptr(stream_meta->_avc0_buffer, r, r1 - r);
            stream_meta->is_sps_reach = true;
          }
          else if (((nalu_type & 0x1f) == 0x08) && ((nalu_type & 0x60) != 0) && !stream_meta->is_pps_reach)
          {
            buffer_append_ptr(stream_meta->_avc0_buffer, h264_split_code, sizeof(h264_split_code));
            buffer_append_ptr(stream_meta->_avc0_buffer, r, r1 - r);
            stream_meta->is_pps_reach = true;
          }

          //if (*(r) == 0x08) {
          //    WRN("recv invalied pps by splitcode  streamid  %s len %d", stream_meta->_stream_id.c_str(), payload_len);
          //}

          r = r1;
        }
      }
      else {
        r = payload;
        while (r < end)
        {
          uint32_t nalu_size = ntohl(*((uint32_t *)r));
          if (r + nalu_size > end)
          {
            DBG("nalu size error %u,skip", nalu_size);
            break;
          }
          uint8_t nalu_type = *(r + 4);
          r += 4;
          if (((nalu_type & 0x1f) == 0x07) && ((nalu_type & 0x60) != 0) && !stream_meta->is_sps_reach)
          {
            buffer_append_ptr(stream_meta->_avc0_buffer, h264_split_code, sizeof(h264_split_code));
            buffer_append_ptr(stream_meta->_avc0_buffer, r, nalu_size);
            stream_meta->is_sps_reach = true;
          }
          else if (((nalu_type & 0x1f) == 0x08) && ((nalu_type & 0x60) != 0) && !stream_meta->is_pps_reach)
          {
            buffer_append_ptr(stream_meta->_avc0_buffer, h264_split_code, sizeof(h264_split_code));
            buffer_append_ptr(stream_meta->_avc0_buffer, r, nalu_size);
            stream_meta->is_pps_reach = true;
          }
          r += nalu_size;

          if (*(r + 4) == 0x08) {
            WRN("recv invalied pps by size num  streamid  %s len %d", stream_meta->_stream_id.c_str(), payload_len);
          }

        }
      }
      _write_header(stream_meta);
    }

    if (stream_meta->is_sps_reach && stream_meta->is_pps_reach && stream_meta->is_adts_reach)
    {
      int tag_len = 0;
      int status = 0;

      if (stream_meta->_last_rtp_video_ts > stream_meta->_video_frame_ts)
      {
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

      if (stream_meta->_first_video_ts == 0)
      {
        stream_meta->_first_video_ts = (uint32_t)FLV::flv_get_timestamp(tag->timestamp);
      }

      FLV::flv_set_timestamp(tag->timestamp, (uint32_t)FLV::flv_get_timestamp(tag->timestamp)
        - stream_meta->_first_video_ts + stream_meta->_video_offset);
      if (stream_meta->_first_audio_ts == 0) {
        stream_meta->_audio_offset = FLV::flv_get_timestamp(tag->timestamp);
      }

      if ((stream_meta->is_check_frame && !stream_meta->is_cur_frame_valied) || ((tag->data[0] & 0x0f) != 7))
      {
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
    if (!stream_meta->_use_nalu_split_code)
    {
      while (!stream_meta->_video_frame_buffer_pos.empty())
      {
        stream_meta->_video_frame_buffer_pos.pop();
      }
    }
  }

  // 白名单通知某个流下线了，目前白名单通知机制被我注释掉了，所以该函数不会被调用
  void RTP2FLVRemuxer::update(const StreamId_Ext &streamid, WhitelistEvent ev) {
    if (ev == WHITE_LIST_EV_STOP)
    {
      META_MAP::iterator it = _meta_map.find(streamid);
      if (it != _meta_map.end())
      {
        delete it->second;
        _meta_map.erase(it);
        INF("stream %s stopped , close rtp2flv remuxer", streamid.unparse().c_str());
      }
    }
  }

  RTP2FLVRemuxer::StreamMeta * RTP2FLVRemuxer::_get_stream_meta_by_sid(const StreamId_Ext& stream_id) {
    META_MAP::iterator it = _meta_map.find(stream_id);
    StreamMeta *result = NULL;
    if (it != _meta_map.end())
    {
      result = it->second;
    }
    else if (SINGLETON(WhitelistManager)->in(stream_id))
    {
      result = _meta_map[stream_id] = new StreamMeta(stream_id);
      INF("recv stream %s pkt  , start rtp2flv remuxer", stream_id.unparse().c_str());
    }
    return result;
  }

  //////////////////////////////////////////////////////////////////////////

  RTPMediaCache *RTP2FLVRemuxer::StreamMeta::get_media_cache() {
    if (_media_cache == NULL)
    {
      int status_code;
      _media_cache = CacheManager::get_rtp_cache_instance()->get_rtp_media_cache(_stream_id, status_code, true);
    }
    return _media_cache;
  }
  RTP2FLVRemuxer::StreamMeta::StreamMeta(StreamId_Ext stream_id) {

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
    _media_cache = NULL;
    _last_video_seq = 0;

    _last_rtp_video_ts = 0;
    _last_rtp_audio_ts = 0;
    _out_sync_ts = 0;


  }
  RTP2FLVRemuxer::StreamMeta::~StreamMeta() {
    if (_flv_header) {
      delete _flv_header;
    }
    if (_flv) {
      delete _flv;
    }
    if (_avc0_buffer)
    {
      buffer_free(_avc0_buffer);
    }
    if (_aac0_buffer)
    {
      buffer_free(_aac0_buffer);
    }
    if (_video_frame_buffer)
    {
      buffer_free(_video_frame_buffer);
    }
    if (_audio_buffer)
    {
      delete _audio_buffer;
    }
    if (_video_buffer)
    {
      delete _video_buffer;
    }

    while (!_audio_out_queue.empty())
    {
      flv_tag *tag = _audio_out_queue.front();
      _audio_out_queue.pop_front();
      if (tag)
      {
        delete tag;
      }
    }
    while (!_video_out_queue.empty())
    {
      flv_tag *tag = _video_out_queue.front();
      _video_out_queue.pop_front();
      if (tag)
      {
        delete tag;
      }
    }
    if (!_use_nalu_split_code)
    {
      while (!_video_frame_buffer_pos.empty())
      {
        _video_frame_buffer_pos.pop();
      }
    }
  }

}
