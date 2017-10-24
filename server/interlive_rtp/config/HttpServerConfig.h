#ifndef _CONFIG_HTTP_SERVER_CONFIG_H_
#define _CONFIG_HTTP_SERVER_CONFIG_H_

#include "config/ConfigManager.h"

class HTTPServerConfig : public ConfigModule {
private:
  bool inited;

public:
  uint16_t listen_port;
  char listen_port_cstr[8];
  int timeout_second;
  int max_conns;
  size_t buffer_max;

public:
  HTTPServerConfig();
  virtual ~HTTPServerConfig();
  HTTPServerConfig& operator=(const HTTPServerConfig& rhv);
  virtual void set_default_config();
  virtual bool load_config(xmlnode* xml_config);
  virtual bool reload() const;
  virtual const char* module_name() const;
  virtual void dump_config() const;
};

#endif
