/*
 * interfaces of forward client and forward server
 * author: hechao@youku.com
 * create: 2013
 *
 */

#ifndef MODULE_BACKEND_
#define MODULE_BACKEND_

#include "config_manager.h"
#include <stdint.h>

class backend_config : public ConfigModule {
private:
  bool inited;

public:
  char backend_listen_ip[32];
  uint16_t backend_listen_port;
  uint16_t backend_asn;
  uint16_t backend_region;
  uint16_t backend_level;
  uint16_t stream_timeout;
  uint32_t forward_client_capability;
  uint32_t forward_client_receive_buf_max_len;
  uint32_t forward_server_max_session_num;
  uint32_t fs_big_session_num;
  uint32_t fs_big_session_size;
  uint32_t fs_connection_buffer_size;

public:
  backend_config();
  virtual ~backend_config();
  backend_config& operator=(const backend_config& rhv);
  virtual void set_default_config();
  virtual bool load_config(xmlnode* xml_config);
  virtual bool reload() const;
  virtual const char* module_name() const;
  virtual void dump_config() const;

private:
  bool load_config_unreloadable(xmlnode* xml_config);
  bool load_config_reloadable(xmlnode* xml_config);
  bool resove_config();
};

#endif
