/*
* Author: lixingyuan01@youku.com
*/

#include "rtp_sender_trans.h"
#include <list>
#include <string>
#include "rtp_trans.h"
#include "rtp_trans_manager.h"


#include "../common/proto_rtp_rtcp.h"


#include "rtp_config.h"

using namespace std;
using namespace avformat;
//using namespace media_manager;


	RTPSendTrans::RTPSendTrans(RtpConnection *connection, StreamId_Ext sid, RTPTransManager *manage, RTPTransConfig* config)
    :RTPTrans(connection, sid, manage, SENDER_TRANS_MODE, config)
	{
    _last_update_packet_lost_rate = 0;
    _frac_packet_lost_rate = 0;
    _rtt_ms = 0;
    _drop_video = false;
    _network_recover_ts = 0;
	}

	RTPSendTrans::~RTPSendTrans()
	{

	}

	void RTPSendTrans::on_handle_rtp(const avformat::RTP_FIXED_HEADER* pkt, uint32_t pkt_len)
	{


		if (pkt && _is_alive)
		{

			if (pkt->get_ssrc())
			{
				uint64_t now = _m_clock->TimeInMilliseconds();
				if (_sessions_map.find(pkt->get_ssrc()) == _sessions_map.end())
				{
					_sessions_map[pkt->get_ssrc()] = new RTPSendSession(this, _config);
					_sessions_array.push_back(_sessions_map[pkt->get_ssrc()]);
				}

				RTPSendSession* session = (RTPSendSession*)_sessions_map[pkt->get_ssrc()];
				if (!session->is_closed())
				{

					session->on_handle_rtp(pkt, pkt_len, now);
					TRC("session->on_handle_rtp");
					_last_keeplive_ts = now;
				}
			}
		}
	}


	void RTPSendTrans::on_handle_rtcp(const uint8_t *data, uint32_t data_len)
	{
		_rtcpPacketInformation.reset();
		    _rtcp->parse_rtcp_packet(data, data_len, _rtcpPacketInformation);

		if ( _rtcpPacketInformation.remoteSSRC && _is_alive)
		{
			uint64_t now = _m_clock->TimeInMilliseconds();
			if (_sessions_map.find(_rtcpPacketInformation.remoteSSRC) == _sessions_map.end())
			{
				_sessions_map[_rtcpPacketInformation.remoteSSRC] = new RTPSendSession(this,  _config);
				_sessions_array.push_back(_sessions_map[_rtcpPacketInformation.remoteSSRC]);
			}

			RTPSendSession* session = (RTPSendSession *)_sessions_map[_rtcpPacketInformation.remoteSSRC];
      if (!session->is_closed())
			{
				session->on_handle_rtcp(&_rtcpPacketInformation, now);
				_last_keeplive_ts = now;
			}
		}
	}
	
  void RTPSendTrans::on_uploader_ntp(avformat::RTPAVType type,
    uint32_t ntp_secs, uint32_t ntp_frac, uint32_t rtp) {
    for (SESSIONS_MAP::iterator it = _sessions_map.begin(); it != _sessions_map.end(); it++) {
      if (it->second->get_payload_type() == type) {
        ((RTPSendSession *)it->second)->on_uploader_ntp(ntp_secs, ntp_frac, rtp);
      }
    }
  }

  void RTPSendTrans::update_packet_lost_rate(uint64_t now) {
    if (_last_update_packet_lost_rate == 0 || now - _last_update_packet_lost_rate > 1000) {
      _last_update_packet_lost_rate = now;

      uint32_t rtt = (uint32_t)-1;
      uint32_t lost = (uint32_t)-1;
      RTPSendSession *session = (RTPSendSession*)get_video_session();
      if (session && session->is_live(now)) {
        rtt = session->get_rtt_ms();
        if (session->is_packet_lost_rate_valid()) {
          lost = session->get_frac_lost_packet_count();
        }
      }
      if (rtt == (uint32_t)-1 || lost == (uint32_t)-1) {
        RTPSendSession *session = (RTPSendSession*)get_audio_session();
        if (session && session->is_live(now)) {
          if (rtt == (uint32_t)-1) {
            rtt = session->get_rtt_ms();
          }
          if (lost == (uint32_t)-1 && session->is_packet_lost_rate_valid()) {
            lost = session->get_frac_lost_packet_count();
          }
        }
      }
      if (rtt != (uint32_t)-1) {
        _rtt_ms = (_rtt_ms + rtt) / 2;
      }
      if (lost != (uint32_t)-1) {
        _frac_packet_lost_rate = (_frac_packet_lost_rate + lost)/2;
      }

      if (_drop_video) {
        bool drop = _frac_packet_lost_rate * 100 / 255 > 30 || _rtt_ms > 500;
        if (drop) {
          _network_recover_ts = 0;
        }
        else if (_network_recover_ts == 0) {
          _network_recover_ts = _m_clock->TimeInMilliseconds();
        }
        else if (_m_clock->TimeInMilliseconds() - _network_recover_ts > 5000) {
          // delay 5 seconds when network recover.
          _drop_video = false;
        }
      }
      else {
        _drop_video = _frac_packet_lost_rate * 100 / 255 > 40 || _rtt_ms > 800;
        _network_recover_ts = 0;
      }
    }
  }
