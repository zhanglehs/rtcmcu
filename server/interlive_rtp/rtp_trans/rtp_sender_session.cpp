/*
 * Author: lixingyuan01@youku.com
 */

#include "rtp_sender_session.h"
#include "rtp_sender_trans.h"

#include "rtp_config.h"

#include "../media_manager/media_manager_state.h"
#include "math.h"
#include <algorithm>
#include <memory>

RTPSendSession::RTPSendSession(RTPTrans * trans, RTPTransConfig* config) :
		RTPSession(trans, SENDER_SESSION_MODE, config) {
  _sending_rtp_packet = false;
  _packet_lost_valid = false;
  _last_send_rtp_ts = 0;
  _start_send_rtp_ts = 0;
}

RTPSendSession::~RTPSendSession() {
}

void RTPSendSession::on_timer(uint64_t now) {
	if (_last_bitrate_ts == 0
			|| now - _last_bitrate_ts > _config->sr_rr_interval) {
		//_effect_bitrate = static_cast<uint32_t>((float)((_frac_rtp_sendbytes - _frac_nack_sendbytes) << 3) / ((float)(now - _last_bitrate_ts) / 1000));
		_send_bitrate = static_cast<uint32_t>((float) ((_total_rtp_sendbytes
				- _last_total_sendbytes) << 3)
				/ ((float) (now - _last_bitrate_ts) / 1000));
		_last_total_sendbytes = _total_rtp_sendbytes;
		_frac_rtp_sendbytes = 0;
		_frac_nack_sendbytes = 0;
		_last_bitrate_ts = now;
	}

  _sending_rtp_packet = now - _last_send_rtp_ts < 3000;
  _packet_lost_valid = _sending_rtp_packet && now - _start_send_rtp_ts > 3000;
}

void RTPSendSession::_on_handle_rtp_f_fec(const avformat::RTP_FIXED_HEADER *pkt,
		uint32_t pkt_len, uint64_t now) {

	if (_config->fec_l > 1 && _config->fec_d_l > 1 && _config->fec_d > 0
			&& pkt->get_seq() > 0) {
		//generate fec pkt
		int r_fec = 1;
		int c_fec = 1;
		r_fec = pkt->get_seq() % _config->fec_l;
		c_fec = (pkt->get_seq() - 1) % _config->fec_d;
		char seqbuf[65536];
		if (_config->log_level <= RTP_TRACE_LEVEL_RTCP)
		{
			memset(seqbuf,0,65536);
		}

		if (!r_fec || !c_fec) {
			memset(_pkt_data, 0, 2048);
			avformat::RTP_FIXED_HEADER *rtp_pkt =
					(avformat::RTP_FIXED_HEADER *) _pkt_data;
			avformat::F_FEC_HEADER *fec_pkt =
					(avformat::F_FEC_HEADER *) (rtp_pkt->data);

			uint16_t rtp_pkt_len = 0;
			uint8_t *fec_data = fec_pkt->data;
			rtp_pkt->version = 2;
			rtp_pkt->marker = 1;
			rtp_pkt->csrc_len = 0;
			rtp_pkt->extension = 0;
			rtp_pkt->padding = 0;
			rtp_pkt->payload = avformat::RTP_AV_F_FEC;

			uint8_t payload = 0;
			uint32_t timestamp = 0;
			uint16_t length = 0;
			uint8_t is_first = 1;
			int32_t status_code = 0;
			uint8_t mark = 0;
			uint16_t seq_base =
					r_fec == 0 ?
							pkt->get_seq() - (_config->fec_l - 1) :
							pkt->get_seq()
									- _config->fec_d * (_config->fec_d_l - 1);

			uint16_t fec_interleave = r_fec == 0 ? 1 : _config->fec_d;
			uint16_t fec_count = r_fec == 0 ? _config->fec_l : _config->fec_d_l;

			for (int i = 0, index = 0; i < fec_count; i++, index +=
					fec_interleave) {
				uint16_t t_len = 0;
				avformat::RTP_FIXED_HEADER *t_pkt =
          _parent->get_rtp_by_ssrc_seq(_payload_type == avformat::RTP_AV_H264,
								static_cast<uint16_t>(seq_base + index), t_len,
								status_code);
				if (media_manager::STATUS_SUCCESS == status_code && t_pkt) {
					if (is_first) {
						payload = t_pkt->payload;
						timestamp = t_pkt->get_rtp_timestamp();
						length = t_len;
						rtp_pkt->set_rtp_timestamp(t_pkt->get_rtp_timestamp());
						rtp_pkt->set_ssrc(_ssrc);
						memcpy(fec_data, t_pkt->data,
								t_len - sizeof(avformat::RTP_FIXED_HEADER));
						is_first = 0;
						rtp_pkt_len = t_len;
						mark = t_pkt->marker;
						//fec_pkt->data[0] = *((uint8_t*)t_pkt);
					} else {
						payload ^= t_pkt->payload;
						timestamp ^= t_pkt->get_rtp_timestamp();
						length ^= t_len;
						//fec_pkt->data[0] ^= *((uint8_t*)t_pkt);
						mark ^= t_pkt->marker;
						if (rtp_pkt_len < t_len) {
							rtp_pkt_len = t_len;
						}
						fec_xor_64(t_pkt->data,
								t_len - sizeof(avformat::RTP_FIXED_HEADER),
								fec_data,
								2048 - sizeof(avformat::RTP_FIXED_HEADER));

					}
					if (_config->log_level <= RTP_TRACE_LEVEL_RTCP)
					{
						char strtemp[32];
						sprintf(strtemp,"%d",t_pkt->get_seq());
						strcat(seqbuf," ");
						strcat(seqbuf,strtemp);
					}
				}
			}
			fec_pkt->marker = mark;
			rtp_pkt->set_seq(
					static_cast<uint16_t>(seq_base
							+ fec_interleave * (fec_count - 1)));
			fec_pkt->set_sn_base(seq_base);
			fec_pkt->set_length_recovery(length);
			fec_pkt->set_payload_type(payload);
			fec_pkt->set_timestamp(timestamp);
			if (!r_fec) {
				fec_pkt->set_M(_config->fec_l);
				fec_pkt->set_N(0);
			} else {
				fec_pkt->set_M(_config->fec_d_l);
				fec_pkt->set_N(_config->fec_d);
			}
			fec_pkt->set_MSK(3);
			_parent->send_fec(_ssrc, rtp_pkt,
					rtp_pkt_len + sizeof(avformat::F_FEC_HEADER));
			_total_rtp_sendbytes += rtp_pkt_len
					+ sizeof(avformat::F_FEC_HEADER);
			if (_config->log_level <= RTP_TRACE_LEVEL_RTCP)
			{
				RTCPL("RTCP_S_FEC streamid %s ssrc %u seq %d payload %d seq %s",_parent->get_sid().unparse().c_str(),_ssrc,0,get_payload_type(),seqbuf);
			}
			//INF("send fec packet ssrc %d byte %d seq %d xor_len %d", _ssrc, fec_pkt->get_mask(), fec_pkt->get_sn_base(), length);
		}
	}
}

void RTPSendSession::_on_handle_rtp_fec(const avformat::RTP_FIXED_HEADER *pkt,
		uint32_t pkt_len, uint64_t now) {
  uint32_t fec_rtp_count = _config->fec_rtp_count;
  if (pkt->payload == avformat::RTP_AV_AAC) {
    fec_rtp_count = FEC_RTP_COUNT;
  }
  else {
    if (((RTPSendTrans*)_parent)->drop_video_rtp_packet()) {
      return;
    }
  }
  if(fec_rtp_count == 0) {
		return;
	}
	if (_config->fec_interleave_package_val > 0) {
		//generate fec pkt
		if (pkt->get_seq() > 0
				&& pkt->get_seq()
						% (_config->fec_interleave_package_val
								* fec_rtp_count) == 0) {

									

					for (unsigned int j = 0; j < _config->fec_interleave_package_val; j++)
					{	
						char seqbuf[65536];
						if (_config->log_level <= RTP_TRACE_LEVEL_RTCP)
						{
							memset(seqbuf,0,65536);
						}
						memset(_pkt_data, 0, 2048);
						avformat::RTP_FIXED_HEADER *rtp_pkt =
								(avformat::RTP_FIXED_HEADER *) _pkt_data;
						avformat::FEC_HEADER *fec_pkt =
								(avformat::FEC_HEADER *) (rtp_pkt->data);
						avformat::FEC_EXTEND_HEADER *fec_ext =
								(avformat::FEC_EXTEND_HEADER *) fec_pkt->data;
						uint16_t rtp_pkt_len = 0;
						uint8_t *fec_data = fec_pkt->data
								+ sizeof(avformat::FEC_EXTEND_HEADER);
						rtp_pkt->version = 2;
						rtp_pkt->marker = 1;
						rtp_pkt->csrc_len = 0;
						rtp_pkt->extension = 0;
						rtp_pkt->padding = 0;
						rtp_pkt->payload = avformat::RTP_AV_FEC;

						uint8_t payload = 0;
						uint32_t timestamp = 0;
						uint16_t length = 0;
						uint8_t is_first = 1;
						int32_t status_code = 0;
						uint32_t mask = 0;
						uint8_t mark = 0;
						uint16_t seq_base = pkt->get_seq() - fec_rtp_count  * _config->fec_interleave_package_val + 1 + j;

						for (unsigned int i = 0; i < fec_rtp_count;
								i++) {
							uint16_t t_len = 0;
							avformat::RTP_FIXED_HEADER *t_pkt =
                _parent->get_rtp_by_ssrc_seq(_payload_type == avformat::RTP_AV_H264,
											static_cast<uint16_t>(seq_base + i *_config->fec_interleave_package_val), t_len,
											status_code);
							if (media_manager::STATUS_SUCCESS == status_code && t_pkt) {
								if (is_first) {
									payload = t_pkt->payload;
									timestamp = t_pkt->get_rtp_timestamp();
									length = t_len;
									rtp_pkt->set_rtp_timestamp(t_pkt->get_rtp_timestamp());
									rtp_pkt->set_ssrc(_ssrc);
									memcpy(fec_data, t_pkt->data,
											t_len - sizeof(avformat::RTP_FIXED_HEADER));
									is_first = 0;
									rtp_pkt_len = t_len;
									mark = t_pkt->marker;
									fec_pkt->data[0] = *((uint8_t*) t_pkt);
								} else {
									payload ^= t_pkt->payload;
									timestamp ^= t_pkt->get_rtp_timestamp();
									length ^= t_len;
									fec_pkt->data[0] ^= *((uint8_t*) t_pkt);
									mark ^= t_pkt->marker;
									if (rtp_pkt_len < t_len) {
										rtp_pkt_len = t_len;
									}
									fec_xor_64(t_pkt->data,
											t_len - sizeof(avformat::RTP_FIXED_HEADER),
											fec_data,
											2048 - sizeof(avformat::RTP_FIXED_HEADER));

								}
								mask |= (1 << (i *_config->fec_interleave_package_val));
								if (_config->log_level <= RTP_TRACE_LEVEL_RTCP)
								{
									char strtemp[32];
									sprintf(strtemp,"%d",t_pkt->get_seq());
									strcat(seqbuf," ");
									strcat(seqbuf,strtemp);
								}

							} else {
								fprintf(stderr,"cur %d seq %d lost \n",pkt->get_seq(), seq_base + i *_config->fec_interleave_package_val);
							}
						}
						fec_ext->marker = mark;
						rtp_pkt->set_seq(
								static_cast<uint16_t>(seq_base + fec_rtp_count - 1));
						fec_pkt->set_sn_base(seq_base);
						fec_pkt->set_length_recovery(length);
						fec_pkt->set_payload_type(payload);
						fec_pkt->set_timestamp(timestamp);
						fec_pkt->set_mask(mask);
						fec_pkt->set_extension(1);
						if (!is_first)
						{
							_parent->send_fec(_ssrc, rtp_pkt,
								rtp_pkt_len + sizeof(avformat::FEC_HEADER)
								+ sizeof(avformat::FEC_EXTEND_HEADER));
							_total_rtp_sendbytes += rtp_pkt_len + sizeof(avformat::FEC_HEADER)+ sizeof(avformat::FEC_EXTEND_HEADER);
							if (_config->log_level <= RTP_TRACE_LEVEL_RTCP)
							{
								RTCPL("RTCP_S_FEC streamid %s ssrc %u seq %d payload %d seq %s",_parent->get_sid().unparse().c_str(),_ssrc,0,get_payload_type(),seqbuf);
							}
						}
					}
					
					
			//INF("send fec packet ssrc %d byte %d seq %d xor_len %d", _ssrc, fec_pkt->get_mask(), fec_pkt->get_sn_base(), length);
		}
	} else {
		_on_handle_rtp_f_fec(pkt, pkt_len, now);
	}
}

void RTPSendSession::_on_handle_rtp_nack(const avformat::RTP_FIXED_HEADER *pkt,
		uint32_t pkt_len, uint64_t now) {
  if (pkt->payload == avformat::RTP_AV_H264 && ((RTPSendTrans*)_parent)->drop_video_rtp_packet()) {
    return;
  }

  uint16_t seq = pkt->get_seq();
  _lastseq = is_newer_seq(_lastseq, seq) ? _lastseq : seq;

  uint32_t time_base = _parent->get_video_time_base();
  if (pkt->payload == avformat::RTP_AV_AAC) {
    time_base = _parent->get_audio_time_base();
  }
  if (time_base > 0 && abs(int(_last_rtp_timestamp
    - pkt->get_rtp_timestamp())) >= int(20 * time_base)) {
    _nacks.clear();
    _lastseq = seq;
  }

  _last_rtp_timestamp = pkt->get_rtp_timestamp();
  _frac_rtp_sendbytes += pkt_len;
  _total_rtp_sendbytes += pkt_len;
  _total_rtp_packetcount++;

  while (isNacklistToolarge() || isNacklistTooOld()) {
    _nacks.erase(_nacks.begin());
  }

  _last_send_rtp_ts = now;
  if (!_sending_rtp_packet) {
    _start_send_rtp_ts = now;
  }
}

uint32_t RTPSendSession::_send_rtcp(avformat::RtcpPacket *pkt) {
	int ret = _parent->send_rtcp(pkt);
	return ret;
}

void RTPSendSession::destroy() {
	if (_parent) {
		avformat::Bye bye;
		bye.From(_ssrc);
		_send_rtcp(&bye);
		DBG("send byte mode send ssrc %u", _ssrc);
	}
}

bool RTPSendSession::is_live(uint64_t now) {
  if (_closed) {
    return false;
  }

	if (_last_keeplive_ts == 0) {
		_last_keeplive_ts = now;
	}

	_timeout = now - _last_keeplive_ts > _config->session_timeout;
  if (_timeout) {
		WRN("rtp session timeout %d", _ssrc);
	}

	return !_timeout;
}

void RTPSendSession::on_handle_rtcp(avformat::RTCPPacketInformation *rtcpPacketInformation, uint64_t now) {
    if (_ssrc == 0) {
      _ssrc = rtcpPacketInformation->remoteSSRC;
    }

    if (rtcpPacketInformation->rtcpPacketTypeFlags & avformat::kRtcpNack && _config->max_nacklst_size > 0) {
      std::vector<uint16_t> nack_items = rtcpPacketInformation->nackList;

      char seq_rtcp[1024];
      char seq_temp[128];
      seq_rtcp[0] = '\0';
      for (std::vector<uint16_t>::iterator it = nack_items.begin(); it != nack_items.end(); it++) {
        uint16_t seq = *it;
        sprintf(seq_temp, "%d,", seq);
        if (strlen(seq_rtcp) < 1000) {
          strcat(seq_rtcp, seq_temp);
        }
      }
      if (_config->log_level <= RTP_TRACE_LEVEL_RTCP) {
        RTCPL("RTCP_S_NACK streamid %s ssrc %u seq %d payload %d seq %s",_parent->get_sid().unparse().c_str(), _ssrc,0,get_payload_type(),seq_rtcp);
      }

      if (_payload_type == avformat::RTP_AV_H264) {
        if (((RTPSendTrans*)_parent)->drop_video_rtp_packet()) {
          return RTPSession::on_handle_rtcp(rtcpPacketInformation, now);
        }
      }

      for (std::vector<uint16_t>::iterator it = nack_items.begin(); it != nack_items.end(); it++) {
        uint16_t seqNum = *it;
        bool drop = _config->max_packet_age && uint32_t(_lastseq - seqNum) > _config->max_packet_age;
        if (drop) {
          continue;
        }

        uint16_t packet_len = 0;
        int32_t status_code = 0;
        avformat::RTP_FIXED_HEADER *packet = _parent->get_rtp_by_ssrc_seq(
          _payload_type == avformat::RTP_AV_H264, seqNum, packet_len, status_code);
        if (packet) {
          uint32_t time_base = _parent->get_video_time_base();
          if (packet->payload == avformat::RTP_AV_AAC) {
            time_base = _parent->get_audio_time_base();
          }
          if (time_base > 0 && abs(int(_last_rtp_timestamp
            - packet->get_rtp_timestamp())) >= int(20 * time_base)) {
            continue;
          }
          unsigned int max_retrans_count = 100;
          if (packet->payload == avformat::RTP_AV_H264) {
            bool key_frame = (packet_len > 18) && (((unsigned char *)packet)[18] >> 7);
            if (key_frame) {
              max_retrans_count = 7;
            }
            else {
              max_retrans_count = 5;
            }
          }
          else {
            max_retrans_count = 9;
          }
          uint32_t interval = ((RTPSendTrans*)_parent)->get_rtt_ms() * 2/3;
          if (interval < _config->min_same_nack_interval) {
            interval = _config->min_same_nack_interval;
          }

          tNackRetransInfo &info = _nacks[seqNum];
          if (info.retrans_count < max_retrans_count && now - info.time > interval) {
            int ret = _parent->SendNackRtp(_payload_type == avformat::RTP_AV_H264, seqNum);
            _frac_nack_sendbytes += ret > 0 ? ret : 0;
            _total_rtp_sendbytes += ret > 0 ? ret : 0;
            info.retrans_count++;
            info.time = now;
          }
        }
      }
    }

    if (rtcpPacketInformation->rtcpPacketTypeFlags & avformat::kRtcpRr) {
      //INF("handle receiver report, ssrc:%u", _ssrc);
      avformat::ReportBlockList report_blocks =
        rtcpPacketInformation->report_blocks;
      uint32_t max_rtt = 0;
      avformat::ReportBlockList::iterator it;
      for (it = report_blocks.begin(); it != report_blocks.end(); it++) {
        avformat::RTCPReportBlock report_block = *(it);
        uint32_t rtt = 0;
        uint32_t delay_rr_ms = report_block.delaySinceLastSR >> 16;
        std::map<uint32_t, uint64_t>::iterator it = _ntps.find(report_block.lastSR);
        if (it != _ntps.end()) {
          int64_t time_used = _m_clock->CurrentNtpInMilliseconds()
            - it->second;
          if (time_used > delay_rr_ms && report_block.lastSR > 0) {
            rtt = (uint32_t)(time_used - delay_rr_ms);
          }
          max_rtt = rtt > max_rtt ? rtt : max_rtt;
        }
        _frac_lost_packet_count = report_block.fractionLost;
        _total_lost_packet_count = report_block.cumulativeLost;
        //sender calc packet lost rate
        _packet_lost_rate = ((float)_frac_lost_packet_count / 255.0);
        DBG("_mode %u ssrc %u frac_lost %u rtt %d", _mode, _ssrc,
          _frac_lost_packet_count, rtt);
        if (_config->enable_suggest_pkt_interval
          && _frac_lost_packet_count
              > 0 && _suggest_pkt_interval < MAX_INTERVAL) {
          _suggest_pkt_interval += PKT_INTERVAL_INCREASE_STEP;
        }
      }
      _rtt_ms = max_rtt;
      if (_config->log_level <= RTP_TRACE_LEVEL_RTCP)
      {
        RTCPL("RTCP_S_RTT _ssrc %d _rtt_ms %d", _ssrc, _rtt_ms);
      }
    }

    RTPSession::on_handle_rtcp(rtcpPacketInformation, now);
}

void RTPSendSession::on_uploader_ntp(uint32_t ntp_secs, uint32_t ntp_frac, uint32_t rtp) {
  _sr_ntp_secs = ntp_secs;
  _sr_ntp_frac = ntp_frac;
  _sr_rtp_ts = rtp;
  send_sr(_m_clock->TimeInMilliseconds());
}

void RTPSendSession::send_sr(uint64_t now) {
  if (_last_process_srts == 0
    || now - _last_process_srts > _config->sr_rr_interval) {
    if (_sr_ntp_secs > 0 || _sr_ntp_frac > 0) {
      avformat::SenderReport sr;
      sr.From(_ssrc);
      sr.WithNtpSec(_sr_ntp_secs);
      sr.WithNtpFrac(_sr_ntp_frac);
      sr.WithRtpTimestamp(_sr_rtp_ts);
      sr.WithOctetCount((uint32_t)_total_rtp_sendbytes);
      sr.WithPacketCount(_total_rtp_packetcount);

      while (_ntps.size() > 10) {
        _ntps.erase(_ntps.begin());
      }
      if (!_ntps.empty() && _ntps.rbegin()->first - _ntps.begin()->first > 60000) {
        _ntps.clear();
      }
      uint32_t ntp_ms = (uint32_t)_m_clock->NtpToMs(_sr_ntp_secs, _sr_ntp_frac);
      _ntps[ntp_ms] = _m_clock->CurrentNtpInMilliseconds();

      avformat::Sdes seds;
      seds.WithCName(_ssrc, RTP_CNAME);
      sr.Append(&seds);
      if (_ssrc) {
        _send_rtcp(&sr);
      }
      _last_process_srts = now;
    }
  }
}
