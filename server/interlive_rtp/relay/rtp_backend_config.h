#ifndef _RTP_BACKEND_CONFIG_H_
#define _RTP_BACKEND_CONFIG_H_

#include "../config_manager.h"
#include "util/xml.h"
#include "rtp_trans/rtp_config.h"

class RtpRelayConfig : public ConfigModule
{
private:
  bool inited;
  bool has_rtp_conf;

public:
  RTPTransConfig _rtp_conf;
  char listen_ip[32];

public:
  RtpRelayConfig();
  virtual ~RtpRelayConfig() {}
  RtpRelayConfig& operator=(const RtpRelayConfig& rhv);

  virtual void set_default_config();
  virtual bool load_config(xmlnode* xml_config);
  virtual bool reload() const;
  virtual const char* module_name() const;
  virtual void dump_config() const;

  bool resove_config();
  bool get_has_rtp_conf() const { return has_rtp_conf; };
  const RTPTransConfig* get_rtp_conf() const
  {
    if (has_rtp_conf)
      return &_rtp_conf;
    else
      return NULL;
  }
};

#endif
