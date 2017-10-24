#ifndef _RTP_PLAYER_CONFIG_H_
#define _RTP_PLAYER_CONFIG_H_

#include "config/ConfigManager.h"
#include "../util/xml.h"
#include "rtp_trans/rtp_config.h"

class RTPPlayerConfig : public ConfigModule
{
private:
  bool inited;
  RTPTransConfig _rtp_conf;

public:
  RTPPlayerConfig();
  virtual ~RTPPlayerConfig() {}
  RTPPlayerConfig& operator=(const RTPPlayerConfig& rhv);

  virtual void set_default_config();
  virtual bool load_config(xmlnode* xml_config);
  virtual bool reload() const;
  virtual const char* module_name() const;
  virtual void dump_config() const;

  RTPTransConfig &get_rtp_trans_config();
};

#endif
