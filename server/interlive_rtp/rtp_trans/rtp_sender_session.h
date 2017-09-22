#pragma once
/*
* Author: lixingyuan01@youku.com
*/
#include "rtp_session.h"



	class RTPSendSession :public RTPSession
	{
	public:
		RTPSendSession(RTPTrans * trans, RTPTransConfig* config);
		~RTPSendSession();

		virtual void on_timer(uint64_t now);
		virtual void destroy();
		virtual bool is_live(uint64_t now);
		virtual void on_handle_rtcp(avformat::RTCPPacketInformation *rtcpPacketInformation, uint64_t now);
    void on_uploader_ntp(uint32_t ntp_secs, uint32_t ntp_frac, uint32_t rtp);
    bool is_packet_lost_rate_valid() { return _packet_lost_valid; }

	protected:
		virtual uint32_t _send_rtcp(avformat::RtcpPacket *pkt);
		virtual void _on_handle_rtp_f_fec(const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len, uint64_t now);
		virtual void _on_handle_rtp_fec(const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len, uint64_t now);
		virtual void _on_handle_rtp_nack(const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len, uint64_t now);
    void send_sr(uint64_t now);

  protected:
    uint32_t _sr_ntp_secs;
    uint32_t _sr_ntp_frac;
    uint32_t _sr_rtp_ts;

    std::map<uint32_t, uint64_t> _ntps;
    bool _sending_rtp_packet;
    bool _packet_lost_valid;
    uint64_t _last_send_rtp_ts;
    uint64_t _start_send_rtp_ts;
	};

