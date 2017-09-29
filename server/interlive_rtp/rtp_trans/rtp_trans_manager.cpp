#include "rtp_trans_manager.h"
//#include "media_manager/media_manager_rtp_interface.h"
#include "media_manager/cache_manager.h"
#include "media_manager/rtp_block_cache.h"
#include <appframe/singleton.hpp>
#include "uploader/RtpTcpConnectionManager.h"

using namespace std;

RTPTransManager::RTPTransManager(RtpCacheManager *helper)
:_mm_helper(helper) {
}

RTPTransManager::~RTPTransManager() {
}

int RTPTransManager::OnRecvRtp(RtpConnection *c, const void *rtp, uint16_t len) {
  if (len > 1500) {
    WRN("rtp packet is larger than 1500.");
  }

  avformat::RTP_FIXED_HEADER *rtp_header = (avformat::RTP_FIXED_HEADER*)rtp;
  uint8_t playload_type = rtp_header->payload;
  if (playload_type != avformat::RTP_AV_FEC
    && playload_type != avformat::RTP_AV_F_FEC) {
    _mm_helper->set_rtp(c->streamid, rtp_header, len);
  }

  c->trans->on_handle_rtp((avformat::RTP_FIXED_HEADER *)rtp, len);

  if (playload_type != avformat::RTP_AV_FEC
    && playload_type != avformat::RTP_AV_F_FEC) {
    // 这种做法有几个问题：
    // 1. 重复包都被转发给player了
    // 2. 通常下行（当前server到player）数比上行数（uploader到当前server）多，
    //    如果较多的下行链路是慢网络（例如当前server的出口带宽不足），
    //    则数据被被copy到了多个RtpConnection::wb中，会导致整体内存占用非常大
    ForwardRtp(c->streamid, rtp_header, len);
  }
  return 0;
}

int RTPTransManager::OnRecvRtcp(RtpConnection *c, const void *rtcp, uint32_t len) {
  c->trans->on_handle_rtcp((const uint8_t*)rtcp, len);

  if (c->IsPlayer()) {
    return 0;
  }

  avformat::RTCPPacketInformation rtcpPacketInformation;
  c->trans->parse_rtcp((const uint8_t *)rtcp,
    len, &rtcpPacketInformation);
  if ((rtcpPacketInformation.rtcpPacketTypeFlags & avformat::kRtcpSr) == 0) {
    return 0;
  }
  avformat::RTPAVType type
    = c->trans->get_av_type(rtcpPacketInformation.remoteSSRC);
  if (type == avformat::RTP_AV_NULL) {
    return 0;
  }

  ForwardRtcp(c->streamid, type,
    rtcpPacketInformation.ntp_secs, rtcpPacketInformation.ntp_frac,
    rtcpPacketInformation.rtp_timestamp);

  return 0;
}

void RTPTransManager::on_timer() {
  uint64_t now = avformat::Clock::GetRealTimeClock()->TimeInMilliseconds();
  for (auto it1 = m_stream_groups.begin(); it1 != m_stream_groups.end(); it1++) {
    auto connections = it1->second;
    for (auto it = connections.begin(); it != connections.end(); it++) {
      RtpConnection *c = *it;
      c->trans->on_timer(now);
    }    
  }
}

int32_t RTPTransManager::set_sdp_char(const StreamId_Ext& stream_id,
  const char* sdp, int32_t len, int32_t& status_code) {
  int32_t ret = _mm_helper->set_sdp(stream_id, sdp, len);

  uint32_t audio_time_base = 0;
  uint32_t video_time_base = 0;
  avformat::SdpInfo sdpinfo;
  sdpinfo.parse_sdp_str(sdp, len);
  std::vector<avformat::rtp_media_info*> medias = sdpinfo.get_media_infos();
  std::vector<avformat::rtp_media_info*>::iterator media_it = medias.begin();
  for (media_it = medias.begin(); media_it != medias.end(); media_it++) {
    avformat::rtp_media_info*  media_info = *(media_it);
    if (media_info->payload_type == avformat::RTP_AV_H264) {
      video_time_base = media_info->rate;
    }
    else if (media_info->payload_type == avformat::RTP_AV_AAC) {
      audio_time_base = media_info->rate;
    }
  }

  auto it1 = m_stream_groups.find(stream_id.get_32bit_stream_id());
  if (it1 == m_stream_groups.end()) {
    return ret;
  }
  std::set<RtpConnection*> &connections = it1->second;
  for (auto it = connections.begin(); it != connections.end(); it++) {
    (*it)->trans->set_time_base(audio_time_base, video_time_base);
  }
  return ret;
}

int32_t RTPTransManager::set_sdp_str(const StreamId_Ext& stream_id,
  const std::string& sdp, int32_t& status_code) {
  return set_sdp_char(stream_id, sdp.c_str(), sdp.length(), status_code);
}

std::string RTPTransManager::get_sdp_str(const StreamId_Ext& stream_id, int32_t& status_code) {
  return _mm_helper->get_sdp(stream_id);
}

int RTPTransManager::_open_trans(RtpConnection *c, const RTPTransConfig *config) {
  RTPTrans *trans;
  if (c->IsPlayer()) {
    trans = new RTPSendTrans(c, c->streamid, this, const_cast<RTPTransConfig*>(config));

    uint32_t audio_time_base = 0;
    uint32_t video_time_base = 0;
    int32_t status_code = 0;
    std::string sdp = get_sdp_str(c->streamid, status_code);
    avformat::SdpInfo sdpinfo;
    sdpinfo.parse_sdp_str(sdp.c_str(), sdp.length());
    std::vector<avformat::rtp_media_info*> medias = sdpinfo.get_media_infos();
    std::vector<avformat::rtp_media_info*>::iterator media_it = medias.begin();
    for (media_it = medias.begin(); media_it != medias.end(); media_it++) {
      avformat::rtp_media_info*  media_info = *(media_it);
      if (media_info->payload_type == avformat::RTP_AV_H264) {
        video_time_base = media_info->rate;
      }
      else if (media_info->payload_type == avformat::RTP_AV_AAC) {
        audio_time_base = media_info->rate;
      }
    }
    trans->set_time_base(audio_time_base, video_time_base);

    while (c->audio_ssrc == 0) {
      c->audio_ssrc = rand();
    }
    while (c->video_ssrc == 0) {
      c->video_ssrc = rand();
    }
  }
  else {
    trans = new RTPRecvTrans(c, c->streamid, this, const_cast<RTPTransConfig*>(config));
  }

  c->trans = trans;

  m_stream_groups[c->streamid.get_32bit_stream_id()].insert(c);
  return 0;
}

avformat::RTP_FIXED_HEADER* RTPTransManager::_get_rtp_by_ssrc_seq(RtpConnection *c,
  bool video, uint16_t seq, uint16_t &len, int32_t& status_code) {
  return _mm_helper->get_rtp_by_seq(c->streamid, video, seq, len);
  //len = 0;
  //if (c->rtp_cache == NULL) {
  //  media_manager::MediaManagerRTPInterface* mm =
  //    media_manager::CacheManager::get_rtp_cache_instance();
  //  int32_t status_code;
  //  c->rtp_cache = mm->get_rtp_media_cache(c->streamid, status_code, true);
  //  if (c->rtp_cache == NULL) {
  //    return NULL;
  //  }
  //}
  //media_manager::RTPCircularCache *cache = NULL;
  //if (video) {
  //  cache = c->rtp_cache->get_video_cache();
  //}
  //else {
  //  cache = c->rtp_cache->get_audio_cache();
  //}
  //if (cache == NULL) {
  //  return NULL;
  //}

  //avformat::RTP_FIXED_HEADER* rtp = cache->get_by_seq(seq, len, status_code);
  //if (!rtp || len > MAX_RTP_LEN) {
  //  return NULL;
  //}
  //return rtp;
}

void RTPTransManager::ForwardRtp(const StreamId_Ext& streamid, avformat::RTP_FIXED_HEADER *rtp, uint32_t len) {
  auto it = m_stream_groups.find(streamid.get_32bit_stream_id());
  if (it == m_stream_groups.end()) {
    return;
  }
  auto connections = it->second;
  for (auto it = connections.begin(); it != connections.end(); it++) {
    RtpConnection *c = *it;
    if (c->IsPlayer()) {
      // hack ssrc for player
      int ssrc = 0;
      if (rtp->payload == avformat::RTP_AV_H264) {
        ssrc = c->video_ssrc;
      }
      else {
        ssrc = c->audio_ssrc;
      }
      rtp->set_ssrc(ssrc);

      c->manager->SendRtp(c, (const unsigned char *)rtp, len);
      c->trans->on_handle_rtp(rtp, len);
    }
  }
}

void RTPTransManager::ForwardRtcp(const StreamId_Ext& streamid, avformat::RTPAVType type, uint32_t ntp_secs, uint32_t ntp_frac, uint32_t rtp) {
  auto it = m_stream_groups.find(streamid.get_32bit_stream_id());
  if (it == m_stream_groups.end()) {
    return;
  }
  auto connections = it->second;
  for (auto it = connections.begin(); it != connections.end(); it++) {
    RtpConnection *c = *it;
    if (c->IsPlayer()) {
      ((RTPSendTrans*)(c->trans))->on_uploader_ntp(type, ntp_secs, ntp_frac, rtp);
    }
  }
}

void RTPTransManager::_close_trans(RtpConnection *c) {
  auto it = m_stream_groups.find(c->streamid.get_32bit_stream_id());
  if (it != m_stream_groups.end()) {
    std::set<RtpConnection*> &connections = it->second;
    connections.erase(c);
    if (connections.empty()) {
      m_stream_groups.erase(it);
    }
  }
}

void RTPTransManager::_send_fec(RtpConnection *c, const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len) {
  c->manager->SendRtp(c, (const unsigned char *)pkt, pkt_len);
}

int RTPTransManager::SendNackRtp(RtpConnection *c, bool video, uint16_t seq) {
  if (!c->udp || !c->IsPlayer()) {
    return -1;
  }

  int32_t code = 0;
  uint16_t rtp_len = 0;
  avformat::RTP_FIXED_HEADER* rtp = _get_rtp_by_ssrc_seq(c, video, seq, rtp_len, code);

  //hack ssrc for player
  uint32_t ssrc = 0;
  if (video) {
    ssrc = c->video_ssrc;
  }
  else {
    ssrc = c->audio_ssrc;
  }
  c->manager->SendRtp(c, (const unsigned char *)rtp, rtp_len, ssrc);
  return 0;
}

void RTPTransManager::_send_rtcp_cb(RtpConnection *c, const avformat::RtcpPacket *rtcp) {
  c->manager->SendRtcp(c, rtcp);
}

void RTPTransManager::_close_trans_cb(RtpConnection *c) {
  //INF("_close_trans_cb. trans_name: %s", trans_name.unparse().c_str());
  // TODO: zhangle, need delete RtpConnection
  _close_trans(c);
}
