#include "rtp_config.h"

#include "../util/log.h"
#include <string.h>

RTPTransConfig::RTPTransConfig()
{
  inited = false;
  set_module_name("rtp_trans");
  set_default_config();
}

RTPTransConfig::~RTPTransConfig()
{
}

RTPTransConfig& RTPTransConfig::operator=(const RTPTransConfig& rhv)
{
  memmove((void*)this, (void*)&rhv, sizeof(RTPTransConfig));
  return *this;
}

void RTPTransConfig::set_default_config()
{
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

bool RTPTransConfig::load_config(xmlnode* xml_config)
{
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
        is_calc_lost_with_retrans = true;
      else
        is_calc_lost_with_retrans = false;
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

void RTPTransConfig::little_down_nack_interval()
{
  if (max_nack_check_interval > 0 && nack_interval > NACK_PROCESS_INERVAL)
  {
    nack_interval -= NACK_PROCESS_INERVAL;
  }
}

void RTPTransConfig::little_upper_nack_interval()
{
  if (max_nack_check_interval > 0 && nack_interval < max_nack_check_interval)
  {
    nack_interval += NACK_PROCESS_INERVAL;
  }
}

bool RTPTransConfig::reload() const
{
  return true;
}

void RTPTransConfig::set_module_name(const char *module_name)
{
  memset(_module_name, 0, sizeof(_module_name));
  strcat(_module_name, module_name);
}

const char* RTPTransConfig::module_name() const
{
  return _module_name;
}

void RTPTransConfig::dump_config() const
{
  INF("rtp trans config: " "max_nack_bandwidth_limit=%d nack_interval=%d max_nacklst_size=%d max_packet_age=%d sr_rr_interval=%d rtt_detect_interval=%d session_timeout=%d fec_l=%d fec_d=%d fec_d_l=%d",
    max_nack_bandwidth_limit, nack_interval, max_nacklst_size, max_packet_age, sr_rr_interval, rtt_detect_interval, session_timeout, fec_l, fec_d, fec_d_l);
}

