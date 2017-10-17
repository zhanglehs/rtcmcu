/*
* Author: lixingyuan01@youku.com
*/

#include "rtp_receiver_trans.h"
#include <list>
#include <string>
#include "rtp_trans.h"
#include "rtp_trans_manager.h"



#include "../common/proto_rtp_rtcp.h"


#include "rtp_config.h"

using namespace std;
using namespace avformat;
//using namespace media_manager;


RTPRecvTrans::RTPRecvTrans(RtpConnection *connection, StreamId_Ext sid, RTPTransManager* manager, RTPTransConfig* config)
		:RTPTrans(connection,sid, manager, RECEIVER_TRANS_MODE, config)
	{

	}

	RTPRecvTrans::~RTPRecvTrans()
	{

	}

	void RTPRecvTrans::on_handle_rtp(const avformat::RTP_FIXED_HEADER* pkt, uint32_t pkt_len)
	{

		if (pkt && _is_alive)
		{
			if (pkt->get_ssrc())
			{
				uint64_t now = _m_clock->TimeInMilliseconds();
				if (_sessions_map.find(pkt->get_ssrc()) == _sessions_map.end())
				{
					_sessions_map[pkt->get_ssrc()] = new RTPRecvSession(this, _config);
					//_sessions_array.push_back(_sessions_map[pkt->get_ssrc()]);
				}

				RTPRecvSession* session = (RTPRecvSession*)_sessions_map[pkt->get_ssrc()];
				if (!session->is_closed())
				{
					session->on_handle_rtp(pkt, pkt_len, now);
					TRC("session->on_handle_rtp");
					_last_keeplive_ts = now;
				}
			}
		}
	}


	void RTPRecvTrans::on_handle_rtcp(const uint8_t *data,uint32_t data_len)
	{

		_rtcpPacketInformation.reset();
	    _rtcp->parse_rtcp_packet(data, data_len, _rtcpPacketInformation);
		if (_rtcpPacketInformation.remoteSSRC && _is_alive)
		{
			uint64_t now = _m_clock->TimeInMilliseconds();
			if (_sessions_map.find(_rtcpPacketInformation.remoteSSRC) == _sessions_map.end())
			{
				_sessions_map[_rtcpPacketInformation.remoteSSRC] = new RTPRecvSession(this, _config);
			}

			RTPRecvSession* session = (RTPRecvSession*) _sessions_map[_rtcpPacketInformation.remoteSSRC];
			if (!session->is_closed())
			{
				session->on_handle_rtcp(&_rtcpPacketInformation, now);
				_last_keeplive_ts = now;
			}
		}
	}


	


