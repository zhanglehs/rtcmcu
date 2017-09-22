#ifndef RTP_SESSION_HEADER
#define RTP_SESSION_HEADER
#include <stdint.h>
#include "../avformat/rtp.h"
#include "../avformat/rtcp.h"
#include "rtp_trans.h"
#include "rtp_config.h"
#include <map>
#include <memory>
//#include <ext/hash_map>
#include <deque>
#include "fragment/rtp_block.h"

typedef std::set<uint16_t> NACK_QUEUE;

class RTPTrans;
struct RTPControlConfig;
enum RTPSessionMode
{
    SENDER_SESSION_MODE,
    RECEIVER_SESSION_MODE
};



class RTPSession
{
public:
    RTPSession(RTPTrans * trans, RTPSessionMode mode, RTPTransConfig *config);
    virtual ~RTPSession();
    //event
    virtual void on_handle_rtp(const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len,uint64_t now); 
    virtual void on_handle_rtcp(avformat::RTCPPacketInformation *rtcpPacketInformation, uint64_t now);
    virtual void on_timer(uint64_t now) = 0;
    //method
    uint32_t get_ssrc();
    uint32_t get_frac_lost_packet_count() { return _frac_lost_packet_count; }
    uint32_t get_total_lost_packet_count() { return _total_lost_packet_count; }
    uint32_t get_total_rtp_packet_count() { return _total_rtp_packetcount; }
    uint32_t get_rtt_ms() { return _rtt_ms; }
    uint32_t get_effect_bitrate() { return _receiver_bitrate; }
    uint32_t get_total_bitrate() { return _send_bitrate; }
    float    get_packet_lost_rate() { return _packet_lost_rate; }
    uint64_t get_suggest_pkt_interval() { return _suggest_pkt_interval; }
    uint64_t get_total_rtp_bytes() { return _total_rtp_sendbytes; }
    avformat::RTPAVType get_payload_type(){ return _payload_type; }
    void set_payload_type(avformat::RTPAVType type){ _payload_type = type; }
    virtual bool is_live(uint64_t now) = 0;
    virtual void destroy() = 0;
    bool is_closed() { return _closed; }

    class SequenceNumberLessThan {
    public:
      bool operator() (const uint16_t& sequence_number1,
        const uint16_t& sequence_number2) const {
        return is_newer_seq(sequence_number2, sequence_number1);
      }
    };

protected:

    static bool is_newer_seq(uint16_t seq, uint16_t preseq);
    bool isNacklistToolarge();
    bool isNacklistTooOld();

    virtual uint32_t _send_rtcp(avformat::RtcpPacket *pkt);
    //virtual uint32_t _send_rtp(uint32_t ssrc, uint16_t seq);
    virtual void _on_handle_rtp_nack(const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len, uint64_t now) = 0;
    virtual void _on_handle_rtp_fec(const avformat::RTP_FIXED_HEADER *pkt, uint32_t pkt_len, uint64_t now) = 0;

    virtual int fec_xor_64(void * src, uint32_t src_len, void *dst, uint32_t dst_len);
    void _set_nacks_array_dirty(uint64_t now);

protected:
    //for sender report
    uint32_t _last_rtp_timestamp;
    uint64_t _total_rtp_sendbytes;
    uint64_t _last_total_sendbytes;
    uint32_t _total_rtp_packetcount;

    uint32_t _frac_nack_sendbytes;
    uint32_t _frac_rtp_sendbytes;
    uint32_t _last_rr_recv_byte;

    //for receiver report
    uint64_t _last_sender_report;
    //statistics
    uint8_t  _frac_lost_packet_count;
    uint32_t _total_lost_packet_count;
    uint32_t _rtt_ms;
    uint32_t _send_bitrate;
	uint32_t _addition_bitrate;

    uint32_t _receiver_bitrate;
    //uint32_t _effect_bitrate;
    float    _packet_lost_rate;
    //need RFC 5401
    //uint32_t recv_bitrate;

    uint64_t _last_process_srts;
    uint64_t _last_process_rrts;
    uint64_t _last_process_remb;
    uint64_t _last_process_nackts;
    uint64_t _last_process_rtt;
    uint64_t _last_keeplive_ts;
    uint64_t _last_bitrate_ts;
    uint64_t _last_nacks_dirty;
    uint64_t _last_nacks_send;
    uint64_t _fec_recover_count;
    uint64_t _retrans_nack_recover_count;
    bool _is_dirty;
    uint16_t _lastseq;
	uint32_t _extend_max_seq;
    //uint32_t since_sr_sendbytes;
    uint64_t _suggest_pkt_interval;

    uint32_t _ssrc;
    RTPTrans *_parent;
    RTPSessionMode _mode;
    //test
    RTPTransConfig *_config;
    bool _closed;
    bool _timeout;
    avformat::RTPAVType _payload_type;
    bool     _pkt_history[65536];
    uint8_t  _pkt_data[2048];
    uint8_t  _xor_tmp_data[2048];

    struct tNackRetransInfo {
      uint64_t time;
      unsigned int retrans_count;
      tNackRetransInfo() : time(0), retrans_count(0) {}
    };
    std::map<uint16_t, tNackRetransInfo, SequenceNumberLessThan> _nacks;

public: 
    avformat::Clock* _m_clock;

};
#endif
