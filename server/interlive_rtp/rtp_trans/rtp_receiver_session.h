#pragma once
/*
* Author: lixingyuan01@youku.com
*/
#include "rtp_session.h"

class RTPRecvSession :public RTPSession
{
public:
	RTPRecvSession(RTPTrans * trans, RTPTransConfig* config);
	~RTPRecvSession();

	virtual void on_timer(uint64_t now);
	void destroy();
	virtual bool is_live(uint64_t now);
  virtual void on_handle_rtcp(avformat::RTCPPacketInformation *rtcpPacketInformation, uint64_t now);

protected:
	virtual uint32_t _send_rtcp(avformat::RtcpPacket *pkt);
	//virtual void _on_handle_rtp_f_fec(const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len, uint64_t now);
	virtual void _on_handle_rtp_fec(const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len, uint64_t now);
	virtual void _on_handle_rtp_nack(const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len, uint64_t now);

	virtual avformat::RTP_FIXED_HEADER * _recover_with_f_fec(const avformat::RTP_FIXED_HEADER *pkt, uint16_t pkt_len, uint16_t& out_len, uint64_t now);
    virtual avformat::RTP_FIXED_HEADER * _recover_with_fec(const avformat::RTP_FIXED_HEADER *pkt, uint16_t pkt_len, uint16_t& out_len, uint64_t now);
    virtual avformat::RTP_FIXED_HEADER * _recover_packet(uint16_t pkt_to_recover, avformat::RTP_FIXED_HEADER * fec_pkt, uint16_t len, uint16_t& out_len);
    virtual avformat::RTP_FIXED_HEADER * _f_recover_packet(uint16_t pkt_to_recover, avformat::RTP_FIXED_HEADER * fec_pkt, uint16_t len, uint16_t& out_len);

    int isHisNacklistTooOld(uint16_t seq);
private:
    typedef std::deque<fragment::RTPBlock *> PENDING_FEC_QUEUE;

    std::set<uint16_t, SequenceNumberLessThan> _his_nacks;
    PENDING_FEC_QUEUE _pending_fec;
    std::map<uint32_t, uint32_t> _sr_history;
};
