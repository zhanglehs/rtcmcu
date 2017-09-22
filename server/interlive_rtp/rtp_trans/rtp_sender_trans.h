#pragma once 
/*
* Author: lixingyuan01@youku.com
*/
#include "rtp_trans.h"
#include "rtp_sender_session.h"



	class RTPSendTrans : public RTPTrans
	{
	public:
    RTPSendTrans(RtpConnection *connection, StreamId_Ext sid, RTPTransManager *manage, RTPTransConfig* config);
		virtual ~RTPSendTrans();

		virtual void on_handle_rtp(const avformat::RTP_FIXED_HEADER* pkt, uint32_t pkt_len);
		virtual void on_handle_rtcp(const uint8_t *data,uint32_t data_len);
    void on_uploader_ntp(avformat::RTPAVType type, uint32_t ntp_secs, uint32_t ntp_frac, uint32_t rtp);

    void update_packet_lost_rate(uint64_t now);
    uint32_t get_frac_packet_lost_rate() { return _frac_packet_lost_rate; }
    uint32_t get_rtt_ms() { return _rtt_ms; }
    //disable drop video now
    bool drop_video_rtp_packet() { return /*_drop_video*/false; }
		
  protected:
    uint64_t _last_update_packet_lost_rate;
    uint32_t _frac_packet_lost_rate;
    uint32_t _rtt_ms;
    bool _drop_video;
    uint64_t _network_recover_ts;
	};

