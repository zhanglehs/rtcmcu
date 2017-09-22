/*
* Author: gaosiyu@youku.com, hechao@youku.com
*/
#include <list>
#include "rtp_trans.h"
#include "rtp_trans_manager.h"
#include "util/log.h"

using namespace std;


void RTPTrans::on_timer(uint64_t now)
{
    SESSIONS_ARRAY::iterator it;
    bool isfirst = true;
    for (it = _sessions_array.begin(); it != _sessions_array.end(); it++)
    {
        RTPSession *session = *(it);
        session->on_timer(now);
        if (isfirst)
        {
            isfirst = false;
            _is_alive = session->is_live(now);
        }
        else
        {
            _is_alive = _is_alive || session->is_live(now);
        }
    }
    if (!_is_alive)
    {
        it = _sessions_array.begin();
        while (it != _sessions_array.end())
        {
            RTPSession *session = *(it);
            _sessions_array.erase(it);
            close_session(session);
        }
    }
    if (_is_alive && now - _last_keeplive_ts > _config->session_timeout)
    {

        if (_sessions_array.empty())
        {
            close_session(NULL);
        }
        else {
            it = _sessions_array.begin();
            while (it != _sessions_array.end())
            {
                RTPSession *session = *(it);
                _sessions_array.erase(it);
                close_session(session);
            }
        }
        
        //INF("no session construct trans will close mode %d streamid %s transname %s", _mode, _sid.unparse().c_str(), _name.unparse().c_str());
    }

    if (_mode == SENDER_TRANS_MODE) {
      ((RTPSendTrans*)this)->update_packet_lost_rate(now);
    }
}

int RTPTrans::SendNackRtp(bool video, uint16_t seq) {
  return _manager->SendNackRtp(m_connection, video, seq);
}

int RTPTrans::send_rtcp(const avformat::RtcpPacket *pkt)
{
  _manager->_send_rtcp_cb(m_connection, pkt);
    return 0;
}

void RTPTrans::close_session(RTPSession *session)
{
    if (session)
    {
        SESSIONS_MAP::iterator it = _sessions_map.find(session->get_ssrc());
        if (it != _sessions_map.end()) {
            _sessions_map.erase(it);
        }
        delete session;
    }

    if (_sessions_array.empty())
    {
        _manager->_close_trans_cb(m_connection);
        //INF("no session remain trans will close mode %d streamid %s transname %s", _mode, _sid.unparse().c_str(), _name.unparse().c_str());
    }
}


void RTPTrans::init_trans_config() 
{
    _config = new RTPTransConfig;
	_config->set_default_config();
}

bool RTPTrans::is_alive()
{
    return _is_alive;
}

void RTPTrans::parse_rtcp(const uint8_t *data, uint32_t data_len, avformat::RTCPPacketInformation *rtcpPacketInformation) {
  rtcpPacketInformation->reset();
  _rtcp->parse_rtcp_packet(data, data_len, *rtcpPacketInformation);
}

avformat::RTPAVType RTPTrans::get_av_type(uint32_t ssrc) {
  for (SESSIONS_MAP::iterator it = _sessions_map.begin(); it != _sessions_map.end(); it++) {
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

void RTPTrans::destroy()
{
    SESSIONS_ARRAY::iterator it = _sessions_array.begin();
    while (it != _sessions_array.end())
    {
        RTPSession *session = *(it);
        session->destroy();
        delete session;
        _sessions_array.erase(it);
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

RTPSession *RTPTrans::get_video_session() 
{
    SESSIONS_ARRAY::iterator it;
    for (it = _sessions_array.begin(); it != _sessions_array.end(); it++)
    {
        RTPSession *session = *(it);
        if (session->is_live(_m_clock->TimeInMilliseconds()))
        {
            switch (session->get_payload_type())
            {
            case avformat::RTP_AV_H264:
                return session;
            default:
                break;
            }
        }
    }
    return NULL;
}

RTPSession *RTPTrans::get_audio_session() 
{
    SESSIONS_ARRAY::iterator it;
    for (it = _sessions_array.begin(); it != _sessions_array.end(); it++)
    {
        RTPSession *session = *(it);
        if (session->is_live(_m_clock->TimeInMilliseconds()))
        {
            switch (session->get_payload_type())
            {
            case avformat::RTP_AV_MP3:
            case avformat::RTP_AV_AAC:
            case avformat::RTP_AV_AAC_GXH:
            case avformat::RTP_AV_AAC_MAIN:
                return session;
            default:
                break;
            }
        }
    }
    return NULL;
}

uint32_t RTPTrans::get_video_frac_lost_packet_count() 
{
    RTPSession *session = get_video_session();
    if (session)
    {
        return session->get_frac_lost_packet_count();
    }
    return 0;
}

uint32_t RTPTrans::get_audio_frac_lost_packet_count()
{
    RTPSession *session = get_audio_session();
    if (session)
    {
        return session->get_frac_lost_packet_count();
    }
    return 0;
}

uint32_t RTPTrans::get_video_total_lost_packet_count() 
{
    RTPSession *session = get_video_session();
    if (session)
    {
        return session->get_total_lost_packet_count();
    }
    return 0;
}

uint32_t RTPTrans::get_audio_total_lost_packet_count()
{
    RTPSession *session = get_audio_session();
    if (session)
    {
        return session->get_total_lost_packet_count();
    }
    return 0;
}

uint32_t RTPTrans::get_video_total_rtp_packet_count() {
    RTPSession *session = get_video_session();
    if (session)
    {
        return session->get_total_rtp_packet_count();
    }
    return 0;
}
uint32_t RTPTrans::get_audio_total_rtp_packet_count() {
    RTPSession *session = get_audio_session();
    if (session)
    {
        return session->get_total_rtp_packet_count();
    }
    return 0;
}


uint32_t RTPTrans::get_video_rtt_ms() 
{
    RTPSession *session = get_video_session();
    if (session)
    {
        return session->get_rtt_ms();
    }
    return 0;
}

uint32_t RTPTrans::get_audio_rtt_ms() 
{
    RTPSession *session = get_audio_session();
    if (session)
    {
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
RTPTrans::~RTPTrans() {
    SESSIONS_ARRAY::iterator it = _sessions_array.begin();
    while (it != _sessions_array.end())
    {
        RTPSession *session = *(it);
        delete session;
        _sessions_array.erase(it);
    }
    _sessions_map.clear();
    if (_rtcp)
    {
        delete _rtcp;
    }
	if (_config)
	{
		delete _config;
	}
}

RTPTransStat RTPTrans::get_stat()
{
    RTPTransStat stat;

    stat.video_frac_lost_packet     = get_video_frac_lost_packet_count();
    stat.audio_frac_lost_packet     = get_audio_frac_lost_packet_count();
    stat.video_total_lost_packet    = get_video_total_lost_packet_count();
    stat.audio_total_lost_packet    = get_audio_total_lost_packet_count();
    stat.video_rtt_ms               = get_video_rtt_ms();
    stat.audio_rtt_ms               = get_audio_rtt_ms();
    stat.video_effect_bitrate       = get_video_effect_bitrate();
    stat.audio_effect_bitrate       = get_audio_effect_bitrate();
    stat.video_total_bitrate        = get_video_total_bitrate();
    stat.audio_total_bitrate        = get_audio_total_bitrate();
    stat.video_total_bytes          = get_video_total_bytes();
    stat.audio_total_bytes          = get_audio_total_bytes();
    stat.video_packet_lost_rate     = get_video_packet_lost_rate();
    stat.audio_packet_lost_rate     = get_audio_packet_lost_rate();
    stat.video_total_rtp_packet     = get_video_total_rtp_packet_count();
    stat.audio_total_rtp_packet     = get_audio_total_rtp_packet_count();
    return stat;
}

//std::string RTPTrans::get_device_id() {
//	return _deviceid;
//}

