#pragma once 
/*
* Author: lixingyuan01@youku.com
*/
#include "rtp_trans.h"
#include "rtp_receiver_session.h"


	class RTPRecvTrans :public RTPTrans
	{
	public:
    RTPRecvTrans(RtpConnection *connection, StreamId_Ext sid, RTPTransManager* manager, RTPTransConfig* config);
		~RTPRecvTrans();
		virtual void on_handle_rtp(const avformat::RTP_FIXED_HEADER* pkt, uint32_t pkt_len);
		virtual void on_handle_rtcp(const uint8_t *data,uint32_t data_len);

	protected:


	private:

	protected:

		friend class RTPRecvSession;
		
	};

