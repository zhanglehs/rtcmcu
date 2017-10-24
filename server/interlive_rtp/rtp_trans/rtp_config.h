#ifndef _RTP_CONFIG_H_
#define _RTP_CONFIG_H_
//#include "stdint.h"
//#include <string.h>
//#include <stdlib.h>
#include "config/ConfigManager.h"
//#include "../util/log.h"

#define MAX_INTERVAL  1000
#define PKT_INTERVAL_INCREASE_STEP 100

#define NACK_LIST_MAX_SIZE 250
#define NACK_PKT_MAX_AGE 450
#define SR_RR_INTERVAL 1000
#define REMB_INTERVAL 1000
#define NACK_PROCESS_INERVAL 10
#define RTT_INERVAL 10
#define RTP_SESSION_TIMEOUT 10000
#define RTP_CNAME "laifeng.com"
#define MAX_NACK_BITRATE 1 << 23
#define ENABLE_SUGGEST_PKT_INTERVAL 0
#define IS_CALC_LOST_WITH_RETRANS 1
#define ENABLE_INNER_JITTERBUF 0
#define MAX_RECONNECT_COUNT 0
#define MIN_SAME_NACK_INTERVAL 30
#define RETRANS_TIMER_RADIO 1.0
#define MAX_NACK_CHECK_INTERVAL 1000
#define FEC_RTP_COUNT 4
#define FEC_INTERLEAVE_PACKAGE_VAL 2

enum RTP_TRACE_LEVEL {
	RTP_TRACE_LEVEL_TRC     = 1,
	RTP_TRACE_LEVEL_RTP     = 2,
	RTP_TRACE_LEVEL_RTCP    = 3,
	RTP_TRACE_LEVEL_RECOVER = 4,
	RTP_TRACE_LEVEL_DBG     = 5,
	RTP_TRACE_LEVEL_INF     = 6,
	RTP_TRACE_LEVEL_WRN     = 7,
	RTP_TRACE_LEVEL_ERR     = 8
};

class RTPTransConfig: public ConfigModule {
private:
	bool inited;
	char _module_name[32];

public:
	uint32_t max_nack_bandwidth_limit;
	uint32_t nack_interval;
	//receiver
	uint32_t max_nacklst_size;
	uint32_t max_packet_age;
	uint32_t sr_rr_interval;
	uint32_t remb_interval;
	uint32_t rtt_detect_interval;
	uint32_t session_timeout;
	uint32_t min_same_nack_interval;
	uint32_t min_nack_interval;
	uint32_t max_nack_check_interval;
	uint32_t fec_rtp_count;
	uint32_t fec_interleave_package_val;
	float fec_threshold;
	uint32_t fec_l;
	uint32_t fec_d;
	uint32_t fec_d_l;

	int32_t max_buffer_duration_ms;
	uint16_t rtp_port;
	bool enable_suggest_pkt_interval;
	bool enable_inner_jitterbuf;

	int max_reconnect_count;
	float retrans_timer_radio;
	bool is_calc_lost_with_retrans;
	RTP_TRACE_LEVEL log_level;

public:
  RTPTransConfig();
  virtual ~RTPTransConfig();
  RTPTransConfig& operator=(const RTPTransConfig& rhv);

  virtual void set_default_config();
  virtual bool load_config(xmlnode* xml_config);
  void little_down_nack_interval();
  void little_upper_nack_interval();
  virtual bool reload() const;
  void set_module_name(const char *module_name);
  virtual const char* module_name() const;
  virtual void dump_config() const;
};
#endif
