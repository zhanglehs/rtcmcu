#ifndef __RTP_TRANS_MANAGER_
#define __RTP_TRANS_MANAGER_

#include <appframe/array_object_pool.hpp>
#include "avformat/sdp.h"
#include "avformat/rtp.h"
//#include "rtp_media_manager_helper.h"
#include "rtp_types.h"
#include "rtp_sender_trans.h"
#include "rtp_receiver_trans.h"
#include "rtp_trans_stat.h"
#include "streamid.h"
#include <deque>
#include <stdint.h>
#include <time.h>

const int MAX_RTP_LEN = (10 * 1024);

class RtpConnection;
class RtpCacheManager;

class RTPTransManager {
  friend class RTPTrans;

public:
  RTPTransManager(RtpCacheManager *helper);
  ~RTPTransManager();

  int _open_trans(RtpConnection *c, const RTPTransConfig *config);
  void _close_trans(RtpConnection *c);

  int OnRecvRtp(RtpConnection *c, const void *rtp, uint16_t len);
  int OnRecvRtcp(RtpConnection *c, const void *rtcp, uint32_t len);

  void on_timer();

  int32_t set_sdp_char(const StreamId_Ext& stream_id, const char* sdp, int32_t len, int32_t& status_code);
  int32_t set_sdp_str(const StreamId_Ext& stream_id, const std::string& sdp, int32_t& status_code);
  std::string get_sdp_str(const StreamId_Ext& stream_id, int32_t& status_code);

protected:
  void _send_fec(RtpConnection *c, const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len);

  // called by rtp_trans
  int SendNackRtp(RtpConnection *c, bool video, uint16_t seq);   // send nack
  void _send_rtcp_cb(RtpConnection *c, const avformat::RtcpPacket *rtcp);
  void _close_trans_cb(RtpConnection *c);

  avformat::RTP_FIXED_HEADER* _get_rtp_by_ssrc_seq(RtpConnection *c,
    bool video, uint16_t seq, uint16_t &len, int32_t& status_code);

  void ForwardRtp(const StreamId_Ext& streamid, avformat::RTP_FIXED_HEADER *rtp, uint32_t len);
  void ForwardRtcp(const StreamId_Ext& streamid, avformat::RTPAVType type, uint32_t ntp_secs, uint32_t ntp_frac, uint32_t rtp);

private:
  static const uint32_t MAX_TRANS_NUM = 5000;
  static const uint32_t MAX_RTP_ITEM_NUM = 25000;

  RtpCacheManager *_mm_helper;

  std::map<uint32_t, std::set<RtpConnection*> > m_stream_groups;
};

#endif
