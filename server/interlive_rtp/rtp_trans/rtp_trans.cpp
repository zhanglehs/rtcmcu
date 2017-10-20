/*
* Author: gaosiyu@youku.com, hechao@youku.com
*/
#include <list>
#include "rtp_trans.h"
#include "rtp_trans_manager.h"
#include "util/log.h"
#include "rtp_sender_trans.h"

void RTPTrans::on_timer(uint64_t now) {
  if (_sessions_map.empty()) {
    if (now - _last_keeplive_ts > _config->session_timeout) {
      _is_alive = false;
    }
  }
  else {
    bool is_alive = false;
    for (auto it = _sessions_map.begin(); it != _sessions_map.end(); it++) {
      RTPSession *session = it->second;
      session->on_timer(now);
      is_alive = is_alive || session->is_live(now);
    }
    _is_alive = is_alive;
  }

  if (_mode == SENDER_TRANS_MODE) {
    ((RTPSendTrans*)this)->update_packet_lost_rate(now);
  }
}

int RTPTrans::SendNackRtp(bool video, uint16_t seq) {
  return _manager->SendNackRtp(m_connection, video, seq);
}

int RTPTrans::send_rtcp(const avformat::RtcpPacket *pkt) {
  _manager->_send_rtcp_cb(m_connection, pkt);
  return 0;
}

void RTPTrans::init_trans_config() {
  _config = new RTPTransConfig;
  _config->set_default_config();
}

bool RTPTrans::is_alive() {
  return _is_alive;
}

void RTPTrans::parse_rtcp(const uint8_t *data, uint32_t data_len, avformat::RTCPPacketInformation *rtcpPacketInformation) {
  rtcpPacketInformation->reset();
  _rtcp->parse_rtcp_packet(data, data_len, *rtcpPacketInformation);
}

avformat::RTPAVType RTPTrans::get_av_type(uint32_t ssrc) {
  for (auto it = _sessions_map.begin(); it != _sessions_map.end(); it++) {
    if (it->second->get_ssrc() == ssrc) {
      return it->second->get_payload_type();
    }
  }
  return avformat::RTP_AV_NULL;
}

void RTPTrans::set_time_base(uint32_t audio_time_base, uint32_t video_time_base) {
  _audio_time_base = audio_time_base;
  _video_time_base = video_time_base;
}

void RTPTrans::updateiTransConfig(RTPTransConfig &config) {
  _config->log_level = config.log_level;
}

RTP_TRACE_LEVEL RTPTrans::get_log_level() {
  return _config->log_level;
}

void RTPTrans::destroy() {
  for (auto it = _sessions_map.begin(); it != _sessions_map.end(); it++) {
    RTPSession *session = it->second;
    session->destroy();
    delete session;
  }
  _sessions_map.clear();
}

avformat::RTP_FIXED_HEADER* RTPTrans::get_rtp_by_ssrc_seq(bool video, uint16_t seq, uint16_t &len, int32_t& status_code) {
  return _manager->_get_rtp_by_ssrc_seq(m_connection, video, seq, len, status_code);
}

//recover
int RTPTrans::recv_rtp(uint32_t ssrc, const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len) {
  _manager->OnRecvRtp(m_connection, pkt, pkt_len);
  return 0;
}

//send fec
int RTPTrans::send_fec(uint32_t ssrc, const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len) {
  _manager->_send_fec(m_connection, pkt, pkt_len);
  return 0;
}

RTPSession *RTPTrans::get_video_session() {
  for (auto it = _sessions_map.begin(); it != _sessions_map.end(); it++) {
    RTPSession *session = it->second;
    if (session->is_live(_m_clock->TimeInMilliseconds())) {
      switch (session->get_payload_type()) {
      case avformat::RTP_AV_H264:
        return session;
      default:
        break;
      }
    }
  }
  return NULL;
}

RTPSession *RTPTrans::get_audio_session() {
  for (auto it = _sessions_map.begin(); it != _sessions_map.end(); it++) {
    RTPSession *session = it->second;
    if (session->is_live(_m_clock->TimeInMilliseconds())) {
      switch (session->get_payload_type()) {
      case avformat::RTP_AV_MP3:
      case avformat::RTP_AV_AAC:
        return session;
      default:
        break;
      }
    }
  }
  return NULL;
}

uint32_t RTPTrans::get_video_frac_lost_packet_count() {
  RTPSession *session = get_video_session();
  if (session) {
    return session->get_frac_lost_packet_count();
  }
  return 0;
}

uint32_t RTPTrans::get_audio_frac_lost_packet_count() {
  RTPSession *session = get_audio_session();
  if (session) {
    return session->get_frac_lost_packet_count();
  }
  return 0;
}

uint32_t RTPTrans::get_video_total_lost_packet_count() {
  RTPSession *session = get_video_session();
  if (session) {
    return session->get_total_lost_packet_count();
  }
  return 0;
}

uint32_t RTPTrans::get_audio_total_lost_packet_count() {
  RTPSession *session = get_audio_session();
  if (session) {
    return session->get_total_lost_packet_count();
  }
  return 0;
}

uint32_t RTPTrans::get_video_total_rtp_packet_count() {
  RTPSession *session = get_video_session();
  if (session) {
    return session->get_total_rtp_packet_count();
  }
  return 0;
}

uint32_t RTPTrans::get_audio_total_rtp_packet_count() {
  RTPSession *session = get_audio_session();
  if (session) {
    return session->get_total_rtp_packet_count();
  }
  return 0;
}

uint32_t RTPTrans::get_video_rtt_ms() {
  RTPSession *session = get_video_session();
  if (session) {
    return session->get_rtt_ms();
  }
  return 0;
}

uint32_t RTPTrans::get_audio_rtt_ms() {
  RTPSession *session = get_audio_session();
  if (session) {
    return session->get_rtt_ms();
  }
  return 0;
}

uint32_t RTPTrans::get_video_effect_bitrate()
{
  RTPSession *session = get_video_session();
  if (session)
  {
    return session->get_effect_bitrate();
  }
  return 0;
}

uint32_t RTPTrans::get_audio_effect_bitrate()
{
  RTPSession *session = get_audio_session();
  if (session)
  {
    return session->get_effect_bitrate();
  }
  return 0;
}

uint32_t RTPTrans::get_video_total_bitrate() {
  RTPSession *session = get_video_session();
  if (session)
  {
    return session->get_total_bitrate();
  }
  return 0;
}

uint32_t RTPTrans::get_audio_total_bitrate() {
  RTPSession *session = get_audio_session();
  if (session)
  {
    return session->get_total_bitrate();
  }
  return 0;
}

float RTPTrans::get_video_packet_lost_rate()
{
  RTPSession *session = get_video_session();
  if (session)
  {
    return session->get_packet_lost_rate();
  }
  return 0;
}

float RTPTrans::get_audio_packet_lost_rate() {
  RTPSession *session = get_audio_session();
  if (session)
  {
    return session->get_packet_lost_rate();
  }
  return 0;
}

uint64_t RTPTrans::get_send_video_pkt_interval()
{
  RTPSession *session = get_video_session();
  if (session)
  {
    return session->get_suggest_pkt_interval();
  }
  return 0;
}

uint64_t RTPTrans::get_send_audio_pkt_interval()
{
  RTPSession *session = get_audio_session();
  if (session)
  {
    return session->get_suggest_pkt_interval();
  }
  return 0;
}

uint64_t RTPTrans::get_video_total_bytes() {
  RTPSession *session = get_video_session();
  if (session)
  {
    return session->get_total_rtp_bytes();
  }
  return 0;
}

uint64_t RTPTrans::get_audio_total_bytes() {
  RTPSession *session = get_audio_session();
  if (session)
  {
    return session->get_total_rtp_bytes();
  }
  return 0;
}

RTPTrans::RTPTrans(RtpConnection* connection, StreamId_Ext sid, RTPTransManager *manager, RTPTransMode mode, RTPTransConfig *config /*= NULL*/)
  :m_connection(connection), _manager(manager), _sid(sid), _mode(mode), _config(config) {
  init_trans_config();
  if (config)
  {
    memcpy(_config, config, sizeof(RTPTransConfig));
  }
  _m_clock = avformat::Clock::GetRealTimeClock();
  _rtcp = new avformat::RTCP(_m_clock);
  _last_keeplive_ts = _m_clock->TimeInMilliseconds();
  _is_alive = true;
  _audio_time_base = 0;
  _video_time_base = 0;
}

RTPTrans::~RTPTrans() {
  for (auto it = _sessions_map.begin(); it != _sessions_map.end(); it++) {
    delete it->second;
  }
  _sessions_map.clear();
  if (_rtcp) {
    delete _rtcp;
  }
  if (_config) {
    delete _config;
  }
}

RTPTransStat RTPTrans::get_stat() {
  RTPTransStat stat;
  stat.video_frac_lost_packet = get_video_frac_lost_packet_count();
  stat.audio_frac_lost_packet = get_audio_frac_lost_packet_count();
  stat.video_total_lost_packet = get_video_total_lost_packet_count();
  stat.audio_total_lost_packet = get_audio_total_lost_packet_count();
  stat.video_rtt_ms = get_video_rtt_ms();
  stat.audio_rtt_ms = get_audio_rtt_ms();
  stat.video_effect_bitrate = get_video_effect_bitrate();
  stat.audio_effect_bitrate = get_audio_effect_bitrate();
  stat.video_total_bitrate = get_video_total_bitrate();
  stat.audio_total_bitrate = get_audio_total_bitrate();
  stat.video_total_bytes = get_video_total_bytes();
  stat.audio_total_bytes = get_audio_total_bytes();
  stat.video_packet_lost_rate = get_video_packet_lost_rate();
  stat.audio_packet_lost_rate = get_audio_packet_lost_rate();
  stat.video_total_rtp_packet = get_video_total_rtp_packet_count();
  stat.audio_total_rtp_packet = get_audio_total_rtp_packet_count();
  return stat;
}
