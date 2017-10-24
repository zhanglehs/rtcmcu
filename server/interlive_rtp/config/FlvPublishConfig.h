#ifndef _CONFIG_FLV_PUBLISH_CONFIG_H_
#define _CONFIG_FLV_PUBLISH_CONFIG_H_

#include "config/ConfigManager.h"

class ModPublisherConfig : public ConfigModule
{
private:
  bool inited;

public:
  char flv_receiver_ip[32];
  uint16_t flv_receiver_port;

  ModPublisherConfig();
  virtual ~ModPublisherConfig() {}
  ModPublisherConfig& operator=(const ModPublisherConfig& rhv);

  virtual void set_default_config();
  virtual bool load_config(xmlnode* xml_config);
  virtual bool reload() const;
  virtual const char* module_name() const;
  virtual void dump_config() const;
};

#endif
