/*
 * Author: lixingyuan01@youku.com
 */

#include "rtp_receiver_session.h"
#include "rtp_receiver_trans.h"

#include "rtp_config.h"

#include "../media_manager/media_manager_state.h"
#include "math.h"
#include <algorithm>
#include <memory>

RTPRecvSession::RTPRecvSession(RTPTrans * trans, RTPTransConfig * config) :
		RTPSession(trans, RECEIVER_SESSION_MODE, config) {

}

RTPRecvSession::~RTPRecvSession() {
    PENDING_FEC_QUEUE::iterator fecp_it = _pending_fec.begin();
    while (fecp_it != _pending_fec.end()) {
      fragment::RTPBlock *block = *(fecp_it);
      block->finalize();
      delete block;
      fecp_it++;
    }
    _pending_fec.clear();
  }

void RTPRecvSession::on_timer(uint64_t now) {
    //process nacks
    if (_config->max_nacklst_size > 0 && (_last_process_nackts == 0
        || now - _last_process_nackts > _config->nack_interval)) {
      if (!_nacks.empty()) {
        //check nacklist size is over 250
        while (isNacklistToolarge()) {
          std::map<uint16_t, tNackRetransInfo, SequenceNumberLessThan>::iterator queue_it = _nacks.begin();
          uint16_t t_seq = queue_it->first;
          _nacks.erase(queue_it);
          if (_config->log_level <= RTP_TRACE_LEVEL_RTP) {
            RTP("RTP_R_NACK_TOOLARGE_DROP streamid %s ssrc %u seq %d payload %d timestamp %u len %d", _parent->_sid.unparse().c_str(), _ssrc, t_seq, _payload_type, 0, 0);
          }
          _set_nacks_array_dirty(now);
        }

        unsigned int max_retrans_count = 30;
        //int lost_percent = (int)_frac_lost_packet_count * 100 / 255;
        //if (_payload_type == avformat::RTP_AV_AAC) {
        //  max_retrans_count = 7;
        //}
        //else {
        //  if (lost_percent < 50){
        //    max_retrans_count = 7;
        //  }
        //  else {
        //    max_retrans_count = 0;
        //  }
        //}

        std::vector<uint16_t> retrans_nack;
        for (std::map<uint16_t, tNackRetransInfo, SequenceNumberLessThan>::iterator it = _nacks.begin(); it != _nacks.end(); it++) {
          uint64_t t_ts = it->second.time;
          if ((t_ts == 0 || now - t_ts > _config->min_same_nack_interval)
            && it->second.retrans_count < max_retrans_count) {
            if (it->second.retrans_count >= 10 && (rand() % 2)) {
              // send nack every 60ms.
              continue;
            }
            retrans_nack.push_back(it->first);
            it->second.time = now;
            it->second.retrans_count++;
          }
        }

        _is_dirty = false;

        //sendrtcppacket
        if (_ssrc && !retrans_nack.empty()) {
          avformat::Nack nack;
          nack.From(_ssrc);
          nack.WithList(&retrans_nack[0], retrans_nack.size());

				for (;;) {
					if (!_is_dirty && _last_nacks_dirty > 0
							&& now - _last_nacks_dirty
									> _config->min_same_nack_interval) {
						break;
					}
					_send_rtcp(&nack);
					break;
				}

          char seq_rtcp[1024];
          char seq_temp[128];
          seq_rtcp[0] = '\0';
          for (unsigned int i = 0; i < retrans_nack.size(); i++) {
            sprintf(seq_temp, "%d,", retrans_nack[i]);
            if (strlen(seq_rtcp) < 1000) {
              strcat(seq_rtcp, seq_temp);
            }
          }
          if (_config->log_level <= RTP_TRACE_LEVEL_RTCP) {
            RTCPL("RTCP_R_NACK streamid %s ssrc %u seq %d payload %d seq %s",_parent->get_sid().unparse().c_str(), _ssrc,0,get_payload_type(),seq_rtcp);
          }
        }

        DBG("rtp session send send nack rtcp pkt %d size %d rtt %d ms ",
            _ssrc, (int)_nacks.size(), _rtt_ms);
      }
      _last_process_nackts = now;
    }
	//process rr
	if (_last_process_rrts == 0
			|| now - _last_process_rrts > _config->sr_rr_interval) {
		avformat::ReportBlock report_block;
		report_block.To(_ssrc);
		report_block.WithCumulativeLost(_total_lost_packet_count);
		report_block.WithFractionLost(_frac_lost_packet_count);
		report_block.WithLastSr(_last_sender_report);
		report_block.WithExtHighestSeqNum(_extend_max_seq);
		// DLSR in unit of 1/65536 seconds
		report_block.WithDelayLastSr((now - _last_process_srts) << 16);
		avformat::ReceiverReport rr;
		rr.From(_ssrc);
		rr.WithReportBlock(&report_block);
		avformat::Sdes seds;
		seds.WithCName(_ssrc, RTP_CNAME);
		rr.Append(&seds);
		if (_ssrc) {
			_send_rtcp(&rr);
		}
		_last_process_rrts = now;
	}

	//REMB
	if (_last_process_remb == 0
			|| now - _last_process_remb > _config->remb_interval) {
		avformat::Remb remb;
		remb.From(_ssrc);
		remb.WithBitrateBps(_send_bitrate);
		if (_ssrc) {
			_send_rtcp(&remb);
		}
		_last_process_remb = now;
	}

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
}

void RTPRecvSession::_on_handle_rtp_fec(const avformat::RTP_FIXED_HEADER *pkt,
		uint32_t pkt_len, uint64_t now) {
	if (pkt->get_seq() < 50 && _lastseq > 65500) {
		memset(_pkt_history, 0, sizeof(bool) * 65536);
		PENDING_FEC_QUEUE::iterator fecp_it = _pending_fec.begin();
		while (fecp_it != _pending_fec.end()) {
			fragment::RTPBlock *block = *(fecp_it);
			block->finalize();
			delete block;
			fecp_it++;
		}
		_pending_fec.clear();
	}
	const avformat::RTP_FIXED_HEADER *new_pkt;
	TRC("get _pkt %d,seq %d,len %d", _ssrc, pkt->get_seq(), pkt_len);
	while (pkt) {

		if (pkt->payload == avformat::RTP_AV_FEC
				|| pkt->payload == avformat::RTP_AV_F_FEC) {
			//INF("gogogogo");

			uint16_t length = 0;
			if (pkt->payload == avformat::RTP_AV_FEC) {
				avformat::FEC_HEADER *fec_pkt = (avformat::FEC_HEADER *)pkt->data;
				

				if (_config->log_level <= RTP_TRACE_LEVEL_RTCP)
				{
					char seqbuf[65536];
					memset(seqbuf,0,65536);
					for (int i = 0; i < 24; i++) {

						/* Count the number of packets needed as well */
						if (fec_pkt->get_mask() >> i & 0x01)
						{
							char strtemp[32];
							sprintf(strtemp,"%d",i + fec_pkt->get_sn_base());
							strcat(seqbuf," ");
							strcat(seqbuf,strtemp);
						}
					}
					RTCPL("RTCP_R_FEC streamid %s ssrc %u seq %d payload %d seq %s",_parent->get_sid().unparse().c_str(),_ssrc,0,get_payload_type(),seqbuf);
				}

				new_pkt = _recover_with_fec(pkt, pkt_len, length, now);
			} else {
				new_pkt = _recover_with_f_fec(pkt, pkt_len, length, now);
			}

			if (new_pkt) {
				_pkt_history[new_pkt->get_seq()] = true;
				TRC("recover _pkt %d,seq %d,len %d", _ssrc, pkt->get_seq(),
						length);
				_parent->recv_rtp(_ssrc, new_pkt, length);
			} else {
				_pending_fec.push_back(new fragment::RTPBlock(pkt, pkt_len));
			}
			pkt = new_pkt;
		} else {
			uint16_t seq = pkt->get_seq();
			_pkt_history[pkt->get_seq()] = true;

			pkt = NULL;

			PENDING_FEC_QUEUE::iterator fecp_it = _pending_fec.begin();
			while (fecp_it != _pending_fec.end()) {
				uint16_t len = 0;
				fragment::RTPBlock *block = *(fecp_it);
				avformat::RTP_FIXED_HEADER *fec = block->get(len);
				if (fec && len > 0) {
					if (static_cast<uint16_t>(seq - fec->get_seq()) < 0x8000) {

						fecp_it = _pending_fec.erase(fecp_it);
						block->finalize();
						delete block;
					} else {

						uint16_t length = 0;
						if (fec->payload == avformat::RTP_AV_FEC) {
							new_pkt = _recover_with_fec(fec, len, length, now);
						} else {
							new_pkt = _recover_with_f_fec(fec, len, length,
									now);
						}
						if (new_pkt) {
							_pkt_history[new_pkt->get_seq()] = true;
							DBG("recover _pkt %d,seq %d,len %d", _ssrc,
									new_pkt->get_seq(), length);
							fecp_it = _pending_fec.erase(fecp_it);
							_parent->recv_rtp(_ssrc, new_pkt, length);
							block->finalize();
							delete block;
							pkt = new_pkt;
							break;
						} else if (length > 0) {
							DBG("delete fec_pkt %d,", _ssrc);
							//								PENDING_FEC_QUEUE::iterator it = fecp_it;
							//								if(it != _pending_fec.end())
							//									it++;
							fecp_it = _pending_fec.erase(fecp_it);
							block->finalize();
							delete block;
							//								fecp_it = it;
						} else {
							fecp_it++;
						}
						//
					}
				}
			}
		}
	}
}

void RTPRecvSession::_on_handle_rtp_nack(const avformat::RTP_FIXED_HEADER *pkt,
		uint32_t pkt_len, uint64_t now) {
	int seq = pkt->get_seq();
	if (_lastseq == 0) {
		_lastseq = seq;
	}

  uint32_t time_base = _parent->get_video_time_base();
  if (pkt->payload == avformat::RTP_AV_AAC) {
    time_base = _parent->get_audio_time_base();
  }
  if (time_base > 0 && abs(int(pkt->get_rtp_timestamp() - _last_rtp_timestamp))
    >= int(20 * time_base)) {
    _his_nacks.clear();
    _nacks.clear();
    if (seq < _lastseq) {
      _extend_max_seq += (uint32_t)(1 << 16);
    }
    _lastseq = seq;
  }
  _last_rtp_timestamp = pkt->get_rtp_timestamp();

  bool redundant = _his_nacks.find(seq) != _his_nacks.end();
  if (redundant) {
    return;
  }
  _his_nacks.insert(seq);
  while (isHisNacklistTooOld(seq)) {
    _his_nacks.erase(_his_nacks.begin());
  }

  bool in_order = is_newer_seq(seq, _lastseq);
  bool retrans = false;
  if (_config->max_nacklst_size > 0) {
    if (in_order) {
      for (uint16_t i = _lastseq + 1; is_newer_seq(seq, i); i++) {
        if (!_pkt_history[i]) {
          _nacks.insert(std::pair<uint16_t, tNackRetransInfo>(i, tNackRetransInfo()));
        }
      }
      _set_nacks_array_dirty(now);
    }
    else {
      std::map<uint16_t, tNackRetransInfo, SequenceNumberLessThan>::iterator it = _nacks.find(seq);
      if (it != _nacks.end()) {
        if (it->second.time > 0) {
          retrans = true;
          if (_config->log_level <= RTP_TRACE_LEVEL_RECOVER) {
            RECOVER("NACK_R_RECOVER streamid %s ssrc %u seq %d payload %d timestamp %u len %d", _parent->_sid.unparse().c_str(), pkt->get_ssrc(), pkt->get_seq(), pkt->payload, pkt->get_rtp_timestamp(), pkt_len);
          }
        }
        else {
          // out of order
        }
        _nacks.erase(it);
      }
    }
  }

  if (retrans) {
    _retrans_nack_recover_count++;
  }
  else {
    _total_rtp_packetcount++;
  }

  if (in_order) {
    if (seq < _lastseq) {
      _extend_max_seq += (uint32_t)(1 << 16);
    }
    _lastseq = seq;
  }
	_extend_max_seq &= 0xffff0000;
	_extend_max_seq |= _lastseq;

	//check nacklist size is over 250
	while (isNacklistToolarge()) {
		std::map<uint16_t, tNackRetransInfo, SequenceNumberLessThan>::iterator queue_it = _nacks.begin();
		uint16_t t_seq = queue_it->first;
		_nacks.erase(queue_it);
		if (_config->log_level <= RTP_TRACE_LEVEL_RTP)
		{
			RTP("RTP_R_NACK_TOOLARGE_DROP streamid %s ssrc %u seq %d payload %d timestamp %u len %d", _parent->_sid.unparse().c_str(), _ssrc, t_seq, _payload_type, 0, 0);
		}
		//_set_nacks_array_dirty(now);
	}

	//check nackitem is delay 450 packet
	while (isNacklistTooOld()) {
		std::map<uint16_t, tNackRetransInfo, SequenceNumberLessThan>::iterator queue_it = _nacks.begin();
		uint16_t t_seq = queue_it->first;
		_nacks.erase(queue_it);
		if (_config->log_level <= RTP_TRACE_LEVEL_RTP)
		{
			RTP("RTP_R_NACK_TOOOLD_DROP streamid %s ssrc %u seq %d payload %d timestamp %u len %d", _parent->_sid.unparse().c_str(), _ssrc, t_seq, _payload_type, 0, 0);
		}
		//_set_nacks_array_dirty(now);
	}

	_last_keeplive_ts = now;
	_frac_rtp_sendbytes += pkt_len;
	_total_rtp_sendbytes += pkt_len;
}

uint32_t RTPRecvSession::_send_rtcp(avformat::RtcpPacket *pkt) {
	int ret = _parent->send_rtcp(pkt);
	return ret;
}

void RTPRecvSession::destroy() {
	if (_parent) {
		avformat::Bye bye;
		bye.From(_ssrc);
		_send_rtcp(&bye);
		DBG("send byte mode recv ssrc %u", _ssrc);
	}
}

bool RTPRecvSession::is_live(uint64_t now) {
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

void RTPRecvSession::on_handle_rtcp(avformat::RTCPPacketInformation *rtcpPacketInformation, uint64_t now) {
  if (_ssrc == 0) {
    _ssrc = rtcpPacketInformation->remoteSSRC;
  }

  if (rtcpPacketInformation->rtcpPacketTypeFlags & avformat::kRtcpSr) {
    _last_process_srts = now;
    _last_sender_report = _m_clock->NtpToMs(rtcpPacketInformation->ntp_secs,
      rtcpPacketInformation->ntp_frac);

    uint32_t sender_count = rtcpPacketInformation->sendPacketCount;
    _total_lost_packet_count = sender_count - _total_rtp_packetcount;
    if (sender_count > _total_rtp_packetcount && sender_count > 0) {
      uint32_t old_lost_count = 0;
      uint32_t old_sender_count = 0;
      for (std::map<uint32_t, uint32_t>::reverse_iterator it = _sr_history.rbegin();
        it != _sr_history.rend(); it++) {
        old_sender_count = it->first;
        old_lost_count = it->second;
        uint32_t min_diff = 100;
        if (_payload_type == avformat::RTP_AV_H264) {
          min_diff = 160;
        }
        if (sender_count - old_sender_count > min_diff) {
          break;
        }
      }

      if (sender_count > old_sender_count) {
        while (_sr_history.size() > 10) {
          _sr_history.erase(_sr_history.begin());
        }
        _sr_history[sender_count] = _total_lost_packet_count;

        _packet_lost_rate = (float)(_total_lost_packet_count - old_lost_count)
          / (sender_count - old_sender_count);
        if ((uint32_t)(_total_lost_packet_count - old_lost_count) > 10000) {
          _packet_lost_rate = 0.0f;
        }
        if (_packet_lost_rate > 1.0f) {
          _packet_lost_rate = 1.0f;
        }
        uint32_t lost = (uint32_t)(_packet_lost_rate * 255);
        if (lost > 255) {
          lost = 255;
        }
        _frac_lost_packet_count = (uint8_t)lost;

        float fec_recover_rate = _fec_recover_count > 0 || _retrans_nack_recover_count > 0
          ? (float)_fec_recover_count / (_fec_recover_count + _retrans_nack_recover_count) : 0;
        float retrans_nack_recover_rate = _fec_recover_count > 0 || _retrans_nack_recover_count > 0
          ? (float)_retrans_nack_recover_count / (_fec_recover_count + _retrans_nack_recover_count) : 0;

        if (_config->log_level <= RTP_TRACE_LEVEL_RTCP) {
          RTCPL("RTCP_R_LOSTRATE frac lost ssrc : %d fec_recover %u, rate %4.4f  allrate %4.4f; retrans_nack %u, rate %4.4f allrate %4.4f; total_lost %u, rate %4.4f bitrate %d rtt %d",
            _ssrc, (unsigned int)_fec_recover_count, fec_recover_rate, fec_recover_rate*_packet_lost_rate, (unsigned int)_retrans_nack_recover_count,
            retrans_nack_recover_rate, retrans_nack_recover_rate*_packet_lost_rate, _total_lost_packet_count, _packet_lost_rate, _send_bitrate, rtcpPacketInformation->rtp_timestamp);
        }
      }
    }
  }

  RTPSession::on_handle_rtcp(rtcpPacketInformation, now);
}

int RTPRecvSession::isHisNacklistTooOld(uint16_t seq) {
  return _config->max_packet_age && !_his_nacks.empty() && uint32_t(seq - *(_his_nacks.begin())) > _config->max_packet_age;
}

avformat::RTP_FIXED_HEADER * RTPRecvSession::_recover_packet(
		uint16_t pkt_to_recover, avformat::RTP_FIXED_HEADER * pkt, uint16_t len,
		uint16_t& out_len) {

	avformat::FEC_HEADER *fec_pkt = (avformat::FEC_HEADER *) (pkt->data);
	avformat::FEC_EXTEND_HEADER *fec_pkt_ext =
			(avformat::FEC_EXTEND_HEADER *) fec_pkt->data;
	//pkt_to_recover seq uint16_t
	memset(_pkt_data, 0, 2048);
	avformat::RTP_FIXED_HEADER *ret = (avformat::RTP_FIXED_HEADER *) _pkt_data;

	ret->set_ssrc(_ssrc);
	ret->set_seq(pkt_to_recover);

	ret->version = 2;

	ret->marker = fec_pkt_ext->marker;
	ret->csrc_len = fec_pkt_ext->csrc_len;
	ret->extension = fec_pkt_ext->extension;
	ret->padding = fec_pkt_ext->padding;

	//payload
	uint8_t payload = fec_pkt->get_payload_type();
	//length  whole
	uint32_t length = fec_pkt->get_length_recovery();
	//timestamp
	uint32_t timestamp = fec_pkt->get_timestamp();
	//data payload except rtp_header
	uint8_t *target_data = (uint8_t *) ret->data;

	//uint16_t seq_base = fec_pkt->get_sn_base();

	// printf("ssrc %d seq %d mask %d \n", _ssrc, seq_base, fec_pkt->get_mask());

	memcpy(target_data, fec_pkt->data + sizeof(avformat::FEC_EXTEND_HEADER),
			len - sizeof(avformat::RTP_FIXED_HEADER)
					- sizeof(avformat::FEC_HEADER)
					- sizeof(avformat::FEC_EXTEND_HEADER));
	int error = 0;
	for (int i = 0; i < 24; i++) {
		if (fec_pkt->get_mask() >> i & 0x01) {
			uint16_t t_len = 0;
			int32_t status_code = 0;
			avformat::RTP_FIXED_HEADER *t_pkt = _parent->get_rtp_by_ssrc_seq(
        _payload_type == avformat::RTP_AV_H264, static_cast<uint16_t>(fec_pkt->get_sn_base() + i),
					t_len, status_code);
			uint8_t *t_data = t_pkt->data;
			if (media_manager::STATUS_SUCCESS == status_code && t_pkt) {

				fec_xor_64(t_data, t_len - sizeof(avformat::RTP_FIXED_HEADER),
						target_data, 2048 - sizeof(avformat::RTP_FIXED_HEADER));

				payload ^= t_pkt->payload;
				length ^= t_len;
				timestamp ^= t_pkt->get_rtp_timestamp();





				ret->marker ^= t_pkt->marker;
				ret->csrc_len ^= t_pkt->csrc_len;
				ret->extension ^= t_pkt->extension;
				ret->padding ^= t_pkt->padding;

				/*
				 {
				 static FILE *file1 = NULL;
				 if (file1 == NULL)
				 {
				 file1 = fopen("/home/gaosiyu/origin.log", "wb+");
				 }
				 static char temp[8192];;
				 char t[8];
				 memset(temp, 0, 8192);

				 sprintf(temp, "process ssrc %d seq %d len %d",
				 t_pkt->get_ssrc(), t_pkt->get_seq(), t_len);

				 for (int i = 0; i < t_len; i++)
				 {
				 memset(t, 0, 8);
				 sprintf(t, " %d", ((uint8_t *)t_pkt)[i]);
				 strcat(temp, t);
				 }
				 strcat(temp, "\r\n");
				 if (((avformat::RTP_FIXED_HEADER *)t_pkt)->payload == avformat::RTP_AV_H264)
				 {
				 fwrite(temp, 1, strlen(temp), file1);
				 fflush(file1);
				 }
				 }

				 {
				 static FILE *file2 = NULL;
				 if (file2 == NULL)
				 {
				 file2 = fopen("/home/gaosiyu/correct.log", "wb+");
				 }
				 static char temp[10240];;
				 char t[8];
				 memset(temp, 0, 10240);

				 sprintf(temp, "process ssrc %d seq %d len %d",
				 pkt->get_ssrc(), pkt->get_seq(), len);

				 for (int i = 0; i < len; i++)
				 {
				 memset(t, 0, 8);
				 sprintf(t, " %d", ((uint8_t *)pkt)[i]);
				 strcat(temp, t);
				 }
				 strcat(temp, "\r\n");
				 if (((avformat::RTP_FIXED_HEADER *)t_pkt)->payload == avformat::RTP_AV_H264)
				 {
				 fwrite(temp, 1, strlen(temp), file2);
				 fflush(file2);
				 }
				 }*/
			} else {
				//printf("get rtp error %d\n", seq_base + i);
				error++;
			}
		}
	}

	if (error < 2  && length > 0 && payload > 0) {
		//assert(((uint8_t *)ret)[1] != 224);

		ret->payload = payload;
		ret->set_rtp_timestamp(timestamp);
		out_len = length;
		_fec_recover_count++;

		if (_config->log_level <= RTP_TRACE_LEVEL_RECOVER)
		{
			RECOVER("FEC_R_RECOVER streamid %s ssrc %u seq %d payload %d timestamp %u len %d",_parent->get_sid().unparse().c_str(),ret->get_ssrc(),ret->get_seq(),ret->payload,ret->get_rtp_timestamp(), out_len);
		}
		_nacks.erase(ret->get_seq());

		//assert(((uint8_t *)ret)[1] != 224);

		/*static FILE *file = NULL;
		 if (file == NULL)
		 {
		 file = fopen("/home/gaosiyu/recover.log", "wb+");
		 }
		 static char temp[8192];;
		 char t[8];
		 memset(temp, 0, 8192);

		 sprintf(temp, "process ssrc %d seq %d len %d pay %d m %d",
		 ret->get_ssrc(), ret->get_seq(), length,
		 ret->payload, ret->marker);
		 for (int i = 0; i < length; i++)
		 {
		 memset(t, 0, 8);
		 sprintf(t, " %d", ((uint8_t *)ret)[i]);
		 strcat(temp, t);
		 }
		 strcat(temp, "\r\n");
		 if (((avformat::RTP_FIXED_HEADER *)ret)->payload == avformat::RTP_AV_H264)
		 {
		 fwrite(temp, 1, strlen(temp), file);
		 fflush(file);
		 }
		 */

		return ret;
	} else {
		return NULL;
	}
}

avformat::RTP_FIXED_HEADER * RTPRecvSession::_f_recover_packet(
		uint16_t pkt_to_recover, avformat::RTP_FIXED_HEADER * pkt, uint16_t len,
		uint16_t& out_len) {
	avformat::F_FEC_HEADER *fec_pkt = (avformat::F_FEC_HEADER *) (pkt->data);
	//pkt_to_recover seq uint16_t
	memset(_pkt_data, 0, 2048);
	avformat::RTP_FIXED_HEADER *ret = (avformat::RTP_FIXED_HEADER *) _pkt_data;

	uint8_t fec_l = fec_pkt->get_M();
	uint8_t fec_d = fec_pkt->get_N();

	TRC("fec_l %d, fec_d %d", fec_l, fec_d);

	ret->set_ssrc(_ssrc);
	ret->set_seq(pkt_to_recover);

	ret->version = 2;
	ret->marker = fec_pkt->marker;
	ret->csrc_len = pkt->csrc_len;
	ret->extension = pkt->extension;
	ret->padding = pkt->padding;

	//payload
	uint8_t payload = fec_pkt->get_payload_type();
	//length  whole
	uint32_t length = fec_pkt->get_length_recovery();
	//timestamp
	uint32_t timestamp = fec_pkt->get_timestamp();
	//data payload except rtp_header
	uint8_t *target_data = (uint8_t *) ret->data;

	memcpy(target_data, fec_pkt->data,
			len - sizeof(avformat::RTP_FIXED_HEADER)
					- sizeof(avformat::F_FEC_HEADER));
	int error = 0;

	if (fec_pkt->get_MSK() == 3) {
		int step = 1;
		if (0 < fec_d && fec_d < 255) {
			step = fec_d;
		}
		for (int i = 0, index = 0; i < fec_l; i++, index += step) {
			uint16_t t_len = 0;
			int32_t status_code = 0;
			avformat::RTP_FIXED_HEADER *t_pkt = _parent->get_rtp_by_ssrc_seq(
        _payload_type == avformat::RTP_AV_H264,
					static_cast<uint16_t>(fec_pkt->get_sn_base() + index),
					t_len, status_code);

			uint8_t *t_data = t_pkt->data;
			if (media_manager::STATUS_SUCCESS == status_code && t_pkt) {
				fec_xor_64(t_data, t_len - sizeof(avformat::RTP_FIXED_HEADER),
						target_data, 2048 - sizeof(avformat::RTP_FIXED_HEADER));

				payload ^= t_pkt->payload;
				length ^= t_len;
				timestamp ^= t_pkt->get_rtp_timestamp();
				TRC(
						"fec recover fec ts %d sn %d,tmp seq %d ts %d,target ts %d,len %d,pt %d",
						fec_pkt->get_timestamp(), index, t_pkt->get_seq(),
						t_pkt->get_rtp_timestamp(), timestamp, length, payload);
				ret->marker ^= t_pkt->marker;
				ret->csrc_len ^= t_pkt->csrc_len;
				ret->extension ^= t_pkt->extension;
				ret->padding ^= t_pkt->padding;
			} else {
				TRC("get rtp error %d", static_cast<uint16_t>(fec_pkt->get_sn_base() + index));
				error++;
			}
		}

	}
	if (error < 2 && length > 0 && payload > 0) {
		//printf("recover ssrc %d seq %d len %d data[0] data[1] data[-2] data[-1]", _ssrc, ret->get_seq(), length, ret->data[0], ret->data[1], ret->data[length - 2], ret->data[length - 1]);
		ret->set_rtp_timestamp(timestamp);
		ret->payload = payload;
		out_len = length;
		_fec_recover_count++;
		_nacks.erase(ret->get_seq());
		return ret;
	} else {
		return NULL;
	}
}

avformat::RTP_FIXED_HEADER * RTPRecvSession::_recover_with_fec(
		const avformat::RTP_FIXED_HEADER *pkt, uint16_t pkt_len,
		uint16_t& out_len, uint64_t now) {

	avformat::RTP_FIXED_HEADER *data_pkt = NULL;
	uint16_t pkts_present, pkts_needed, pkt_to_recover, i;

	pkts_present = 0;
	pkts_needed = -1;

	avformat::FEC_HEADER *fec_pkt = (avformat::FEC_HEADER *) pkt->data;





	for (i = 0; i < 24; i++) {

		/* If the packet is here and in the mask, increment counter */
		if (_pkt_history[i + fec_pkt->get_sn_base()]
				&& fec_pkt->get_mask() >> i & 0x01) {
			pkts_present++;
		}

		/* Count the number of packets needed as well */
		if (fec_pkt->get_mask() >> i & 0x01) {
			pkts_needed++;

		}

		/* The packet to recover is the one with a bit in the
		 mask that's not here yet */
		if (!_pkt_history[i + fec_pkt->get_sn_base()]
				&& fec_pkt->get_mask() >> i & 0x01)
			pkt_to_recover = i + fec_pkt->get_sn_base();
	}


	/* If we can recover, do so. Otherwise, return NULL */

	if (pkts_present == pkts_needed) {
		//printf("try to recover pkt %d \n", pkt_to_recover);
		data_pkt = _recover_packet(pkt_to_recover,
				(avformat::RTP_FIXED_HEADER *) pkt, pkt_len, out_len);
	} else {
		data_pkt = NULL;
	}

	//printf("recover %d pointer %d\n", pkt_to_recover, data_pkt);

	return (data_pkt);
}

avformat::RTP_FIXED_HEADER * RTPRecvSession::_recover_with_f_fec(
		const avformat::RTP_FIXED_HEADER *pkt, uint16_t pkt_len,
		uint16_t& out_len, uint64_t now) {
	avformat::RTP_FIXED_HEADER *data_pkt = NULL;
	uint16_t pkts_present = 0, pkts_needed, pkt_to_recover, needed_packet_seq;
	uint8_t fec_l = 0;
	uint8_t fec_d = 0;
	//uint8_t fec_d_l = 0;

	avformat::F_FEC_HEADER *fec_pkt = (avformat::F_FEC_HEADER *) pkt->data;
	TRC("fec sn %d , ", fec_pkt->get_sn_base());
	if (fec_pkt->get_MSK() == 3) {
		fec_l = fec_pkt->get_M();
		fec_d = fec_pkt->get_N();
		TRC("fec_l %d, fec_d %d", fec_l, fec_d);
		int step = 1;
		if (0 < fec_d && fec_d < 255) {
			step = fec_d;
		}
		char seqbuf[65536];

		if (_config->log_level <= RTP_TRACE_LEVEL_RTCP)
		{
			memset(seqbuf,0,65536);
		}
		for (int i = 0, index = 0; i < fec_l; i++, index += step) {
			needed_packet_seq = static_cast<uint16_t>(index + fec_pkt->get_sn_base());
			TRC("_pkt_history %d,%d,%d", _ssrc, needed_packet_seq,
					_pkt_history[needed_packet_seq]);
			/* If the packet is here and in the mask, increment counter */
			if (_pkt_history[needed_packet_seq]) {
				if (_config->log_level <= RTP_TRACE_LEVEL_RTCP)
				{
					char strtemp[32];
					sprintf(strtemp,"%d",needed_packet_seq);
					strcat(seqbuf," ");
					strcat(seqbuf,strtemp);
				}
				pkts_present++;
			} else {
				/* The packet to recover is the one with a bit in the
				 mask that's not here yet */
				pkt_to_recover = needed_packet_seq;
			}
		}

		if (_config->log_level <= RTP_TRACE_LEVEL_RTCP)
		{
			RTCPL("RTCP_R_FEC streamid %s ssrc %u seq %d payload %d seq %s",_parent->get_sid().unparse().c_str(),_ssrc,0,get_payload_type(),seqbuf);
		}

		/* Count the number of packets needed as well */
		pkts_needed = fec_l - 1;
		/* If we can recover, do so. Otherwise, return NULL */
		if (pkts_present == pkts_needed) {
			data_pkt = _f_recover_packet(pkt_to_recover,
					(avformat::RTP_FIXED_HEADER *) pkt, pkt_len, out_len);
		} else if (pkts_present == fec_l) {
			out_len = fec_l;
		}

	}
	return (data_pkt);
}
