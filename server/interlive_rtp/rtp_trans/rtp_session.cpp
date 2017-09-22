#include "rtp_session.h"

#include "util/log.h"
#include "math.h"

#include "../avformat/rtp.h"
#include "../fragment/rtp_block.h"
#include "media_manager/media_manager_state.h"

RTPSession::RTPSession(RTPTrans * trans, RTPSessionMode mode,
		RTPTransConfig *config) {
	_parent = trans;
	_mode = mode;
	_last_process_srts = 0;
	_last_process_rrts = 0;
	_last_process_remb = 0;
	_last_process_nackts = 0;
	_last_process_rtt = 0;
	_last_keeplive_ts = 0;
	_last_bitrate_ts = 0;

	_last_rtp_timestamp = 0;
	_total_rtp_sendbytes = 0;
	_total_rtp_packetcount = 0;
	_last_sender_report = 0;
	_frac_nack_sendbytes = 0;
	_frac_rtp_sendbytes = 0;
	_last_total_sendbytes = 0;

	_frac_lost_packet_count = 0;
	_total_lost_packet_count = 0;
	_suggest_pkt_interval = 0;

	_last_nacks_dirty = 0;
	_last_nacks_send = 0;

	_fec_recover_count = 0;
	_retrans_nack_recover_count = 0;

	_lastseq = 0;
	_extend_max_seq = 0;
	_send_bitrate = 0;
	_receiver_bitrate = 0;
	//_effect_bitrate = 0;
	_rtt_ms = 0;
	_ssrc = 0;
	_packet_lost_rate = 0;
	_is_dirty = true;
	//_alive = true;
  _closed = false;
  _timeout = false;
	_m_clock = avformat::Clock::GetRealTimeClock();
	_payload_type = avformat::RTP_AV_NULL;
	this->_config = config;

	memset(_pkt_history, 0, sizeof(bool) * 65536);
}

RTPSession::~RTPSession() {
}

void RTPSession::on_handle_rtp(const avformat::RTP_FIXED_HEADER *pkt,
  uint32_t pkt_len, uint64_t now) {
  if (_ssrc == 0) {
    _ssrc = pkt->get_ssrc();
  }
  if (_payload_type == avformat::RTP_AV_NULL
    && pkt->payload != avformat::RTP_AV_FEC
    && pkt->payload != avformat::RTP_AV_F_FEC) {
    _payload_type = (avformat::RTPAVType) pkt->payload;
  }

  _on_handle_rtp_fec(pkt, pkt_len, now);

  if (pkt->payload != avformat::RTP_AV_FEC
    && pkt->payload != avformat::RTP_AV_F_FEC) {
    _on_handle_rtp_nack(pkt, pkt_len, now);
  }
}

int RTPSession::fec_xor_64(void * src, uint32_t src_len, void *dst,
		uint32_t dst_len) {
	memset(_xor_tmp_data, 0, 2048);
	memcpy(_xor_tmp_data, src, src_len);
	uint64_t *psrc64 = (uint64_t*) _xor_tmp_data;
	uint64_t *pdst64 = (uint64_t*) dst;
  for (uint32_t j = 0; j < (std::max(src_len, dst_len) + 7) >> 3; j++) {
		if (j < src_len) {
			*pdst64++ ^= *psrc64++;
		} else {
			*pdst64++ ^= 0;
		}
	}

	return 0;
}

#include <vector>

void RTPSession::on_handle_rtcp(
		avformat::RTCPPacketInformation *rtcpPacketInformation, uint64_t now) {
	if (_ssrc == 0) {
		_ssrc = rtcpPacketInformation->remoteSSRC;
	}

	if (rtcpPacketInformation->rtcpPacketTypeFlags & avformat::kRtcpRemb) {
		_receiver_bitrate = rtcpPacketInformation->receiverEstimatedMaxBitrate;
	}

	if (rtcpPacketInformation->rtcpPacketTypeFlags & avformat::kRtcpBye) {
    _closed = true;
	}

	_last_keeplive_ts = now;
}

bool RTPSession::is_newer_seq(uint16_t seq, uint16_t preseq) {
	return (seq != preseq) && static_cast<uint16_t>(seq - preseq) < 0x8000;
}

bool RTPSession::isNacklistToolarge() {
  return _config->max_nacklst_size && _nacks.size() > _config->max_nacklst_size;
}

bool RTPSession::isNacklistTooOld() {
  return _config->max_packet_age && !_nacks.empty() && uint32_t(_lastseq - _nacks.begin()->first) > _config->max_packet_age;
}

uint32_t RTPSession::get_ssrc() {
	return _ssrc;
}

uint32_t RTPSession::_send_rtcp(avformat::RtcpPacket *pkt) {
	int ret = _parent->send_rtcp(pkt);
	return ret;
}

//uint32_t RTPSession::_send_rtp(uint32_t ssrc, uint16_t seq) {
//	int ret = _parent->send_rtp(ssrc, seq);
//	return ret;
//}


void RTPSession::_set_nacks_array_dirty(uint64_t now) {
	_is_dirty = TRUE;
	_last_nacks_dirty = now;
}
