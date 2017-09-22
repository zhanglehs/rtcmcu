/*
* Author: gaosiyu@youku.com, hechao@youku.com
*/

#ifndef __RTP_TRANS_
#define __RTP_TRANS_

#include <map>
#include <memory>
//#include <ext/hash_map>
#include "../avformat/rtp.h"
#include "../avformat/rtcp.h"
#include "rtp_session.h"
#include "../streamid.h"
#include "rtp_config.h"
//#include "rtp_trans_id.h"
#include "rtp_trans_stat.h"


enum RTPTransMode
{
  SENDER_TRANS_MODE,
  RECEIVER_TRANS_MODE
};


class RTPTransConfig;
class RTPSession;
class RTPTransManager;
class RtpConnection;


class RTPTrans
{
public:

  RTPTrans(RtpConnection* connection, StreamId_Ext sid, RTPTransManager *manager, RTPTransMode mode, RTPTransConfig *config = NULL)
    :m_connection(connection), _manager(manager), _sid(sid), _mode(mode), _config(config)
  {

    init_trans_config();
    if (config)
    {
      memcpy(_config, config, sizeof(RTPTransConfig));
    }
    _m_clock = avformat::Clock::GetRealTimeClock();
    _rtcp = new avformat::RTCP(_m_clock);
    _last_keeplive_ts = _m_clock->TimeInMilliseconds();
    _is_alive = true;
    _audio_time_base = 0;
    _video_time_base = 0;
  }

  virtual ~RTPTrans();

public:
  virtual void on_handle_rtp(const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len) = 0;
  virtual void on_handle_rtcp(const uint8_t *data, uint32_t data_len) = 0;
  virtual void on_timer(uint64_t now);
  virtual void destroy();
  RTPTransMode get_mode() { return _mode; }
  bool is_alive();
  void parse_rtcp(const uint8_t *data, uint32_t data_len, avformat::RTCPPacketInformation *rtcpPacketInformation);
  avformat::RTPAVType get_av_type(uint32_t ssrc);
  void set_time_base(uint32_t audio_time_base, uint32_t video_time_base);
  uint32_t get_audio_time_base() { return _audio_time_base; }
  uint32_t get_video_time_base() { return _video_time_base; }

  void updateiTransConfig(RTPTransConfig &config);

  RTP_TRACE_LEVEL get_log_level();

  //RTPTransId& get_name() { return _name; }
  const StreamId_Ext &get_sid() const { return _sid; }


  uint64_t get_send_video_pkt_interval();
  uint64_t get_send_audio_pkt_interval();


  uint32_t get_video_frac_lost_packet_count();
  uint32_t get_audio_frac_lost_packet_count();
  uint32_t get_video_total_lost_packet_count();
  uint32_t get_audio_total_lost_packet_count();
  uint32_t get_video_total_rtp_packet_count();
  uint32_t get_audio_total_rtp_packet_count();
  uint32_t get_video_rtt_ms();
  uint32_t get_audio_rtt_ms();
  uint32_t get_video_effect_bitrate();
  uint32_t get_audio_effect_bitrate();
  uint32_t get_video_total_bitrate();
  uint32_t get_audio_total_bitrate();
  uint64_t get_video_total_bytes();
  uint64_t get_audio_total_bytes();
  float    get_video_packet_lost_rate();
  float    get_audio_packet_lost_rate();

  RTPTransStat get_stat();

  //std::string get_device_id();

protected:

  friend class RTPSession;
  friend class RTPRecvSession;
  friend class RTPSendSession;
  virtual int SendNackRtp(bool video, uint16_t seq);
  virtual int send_fec(uint32_t ssrc, const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len);
  virtual int send_rtcp(const avformat::RtcpPacket *pkt);
  virtual int recv_rtp(uint32_t ssrc, const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len);
  virtual avformat::RTP_FIXED_HEADER* get_rtp_by_ssrc_seq(bool video, uint16_t seq, uint16_t &len, int32_t& status_code);
  virtual void close_session(RTPSession *session);
  RTPSession *get_video_session();
  RTPSession *get_audio_session();
  void init_trans_config();

protected:
  RtpConnection *m_connection;
  RTPTransManager * _manager;
  StreamId_Ext _sid;
  uint64_t _last_keeplive_ts;
  avformat::Clock* _m_clock;
  avformat::RTCPPacketInformation _rtcpPacketInformation;
  avformat::RTCP *_rtcp;

protected:
  typedef __gnu_cxx::hash_map<uint32_t, RTPSession *> SESSIONS_MAP;
  typedef std::vector<RTPSession *> SESSIONS_ARRAY;
  SESSIONS_MAP _sessions_map;
  SESSIONS_ARRAY _sessions_array;
  RTPTransMode _mode;
  RTPTransConfig *_config;
  //std::string _deviceid;
  bool _is_alive;
  uint32_t _audio_time_base;
  uint32_t _video_time_base;
};

#endif

