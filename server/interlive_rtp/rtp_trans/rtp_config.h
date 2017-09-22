#ifndef RTP_CONFIG_HEADER
#define RTP_CONFIG_HEADER
#include "stdint.h"
#include <string.h>
#include <stdlib.h>
#ifndef  LIVESTREAMSDK_EXPORTS
#include "../config_manager.h"
#include "../../util/log.h"
#endif

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

class RTPTransConfig
#ifndef  LIVESTREAMSDK_EXPORTS
: public ConfigModule
#endif
{
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
	RTPTransConfig() {
		inited = false;
		set_module_name("rtp_trans");
		set_default_config();
	}
	virtual ~RTPTransConfig() {

	}
	RTPTransConfig& operator=(const RTPTransConfig& rhv) {
		memmove((void*)this, (void*)&rhv, sizeof(RTPTransConfig));
		return *this;
	}
	virtual void set_default_config() {
		max_nack_bandwidth_limit = MAX_NACK_BITRATE;
		nack_interval = NACK_PROCESS_INERVAL;
		max_nacklst_size = NACK_LIST_MAX_SIZE;
		max_packet_age = NACK_PKT_MAX_AGE;
		sr_rr_interval = SR_RR_INTERVAL;
		remb_interval = REMB_INTERVAL;
		rtt_detect_interval = RTT_INERVAL;
		session_timeout = RTP_SESSION_TIMEOUT;
		enable_suggest_pkt_interval = ENABLE_SUGGEST_PKT_INTERVAL;
		enable_inner_jitterbuf = ENABLE_INNER_JITTERBUF;
		fec_interleave_package_val = FEC_INTERLEAVE_PACKAGE_VAL;
		max_reconnect_count = MAX_RECONNECT_COUNT;
		min_same_nack_interval = MIN_SAME_NACK_INTERVAL;
		max_nack_check_interval = MAX_NACK_CHECK_INTERVAL;
		retrans_timer_radio = RETRANS_TIMER_RADIO;
		is_calc_lost_with_retrans = IS_CALC_LOST_WITH_RETRANS;
		fec_rtp_count = FEC_RTP_COUNT;
		fec_l = 4;
		fec_d = 4;
		fec_d_l = 4;
		log_level = RTP_TRACE_LEVEL_ERR;
	}

#ifndef  LIVESTREAMSDK_EXPORTS
	virtual bool load_config(xmlnode* xml_config) {
		//xmlnode *p = xmlgetchild(xml_config, _module_name, 0);
		xmlnode *p = xml_config;
		const char *q = NULL;
		if (p)
		{
			p = xmlgetchild(p, _module_name, 0);

			q = xmlgetattr(p, "max_nack_bandwidth_limit");
			if (q)
			{
				max_nack_bandwidth_limit = atoi(q);
			}

			q = xmlgetattr(p, "nack_interval");
			if (q)
			{
				nack_interval = atoi(q);
			}

			q = xmlgetattr(p, "max_nacklst_size");
			if (q)
			{
				max_nacklst_size = atoi(q);
			}

			q = xmlgetattr(p, "max_packet_age");
			if (q)
			{
				max_packet_age = atoi(q);
			}

			q = xmlgetattr(p, "sr_rr_interval");
			if (q)
			{
				sr_rr_interval = atoi(q);
			}

			q = xmlgetattr(p, "remb_interval");
			if (q)
			{
				remb_interval = atoi(q);
			}

			q = xmlgetattr(p, "rtt_detect_interval");
			if (q)
			{
				rtt_detect_interval = atoi(q);
			}

			q = xmlgetattr(p, "session_timeout");
			if (q)
			{
				session_timeout = atoi(q);
			}

			q = xmlgetattr(p, "retrans_radio");
			if (q)
			{
				retrans_timer_radio = atof(q);
			}

			q = xmlgetattr(p, "max_nack_check_interval");
			if (q)
			{
				max_nack_check_interval = atoi(q);
			}

			q = xmlgetattr(xml_config, "is_calc_lost_with_retrans");
			if (q)
			{
				if (strncmp("TRUE", q, 4) == 0)
				is_calc_lost_with_retrans = TRUE;
				else
				is_calc_lost_with_retrans = FALSE;
			}
                        
                        q = xmlgetattr(p, "fec_rtp_count");
			if (q)
			{
				fec_rtp_count = atoi(q);
			}

			q = xmlgetattr(p, "fec_interleave_package_val");
			if (q)
			{
				fec_interleave_package_val = atoi(q);
			}

			q = xmlgetattr(p, "fec_l_val");
			if (q)
			{
				fec_l = atoi(q);
			}

			q = xmlgetattr(p, "fec_d_val");
			if (q)
			{
				fec_d = atoi(q);
			}

			q = xmlgetattr(p, "fec_d_l_val");
			if (q)
			{
				fec_d_l = atoi(q);
			}
			//DBG("read config fec_l %d, fec_d %d, fec_d_l %d", fec_l,fec_d,fec_d_l);
			return true;
		}
		return false;
	}
#endif

	void little_down_nack_interval() {
		if (max_nack_check_interval > 0 && nack_interval > NACK_PROCESS_INERVAL)
		{
			nack_interval -= NACK_PROCESS_INERVAL;
		}
	}

	void little_upper_nack_interval() {
		if (max_nack_check_interval > 0 && nack_interval < max_nack_check_interval)
		{
			nack_interval += NACK_PROCESS_INERVAL;
		}
	}

	virtual bool reload() const {
		return true;
	}

	void set_module_name(const char *module_name) {
		memset(_module_name, 0, sizeof(_module_name));
		strcat(_module_name, module_name);
	}

	virtual const char* module_name() const {
		return _module_name;
	}
	virtual void dump_config() const {
#ifndef  LIVESTREAMSDK_EXPORTS
		INF("rtp trans config: " "max_nack_bandwidth_limit=%d nack_interval=%d max_nacklst_size=%d max_packet_age=%d sr_rr_interval=%d rtt_detect_interval=%d session_timeout=%d fec_l=%d fec_d=%d fec_d_l=%d",
				max_nack_bandwidth_limit, nack_interval, max_nacklst_size, max_packet_age, sr_rr_interval, rtt_detect_interval, session_timeout, fec_l,fec_d,fec_d_l);
#endif
	}
};
#endif
