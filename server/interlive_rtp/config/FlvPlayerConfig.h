#ifndef _CONFIG_FLV_PLAYER_CONFIG_H_
#define _CONFIG_FLV_PLAYER_CONFIG_H_

#include "config/ConfigManager.h"

class FlvPlayerConfig : public ConfigModule
{
private:
  bool inited;

public:
  char player_listen_ip[32];
  uint16_t player_listen_port;
  int stuck_long_duration;
  int timeout_second;
  int max_players;
  size_t buffer_max;
  bool always_generate_hls;
  uint32_t ts_target_duration;
  uint32_t ts_segment_cnt;
  char private_key[64];
  char super_key[36];
  int sock_snd_buf_size;
  int normal_offset;
  int latency_offset;
  std::string crossdomain_path;
  char crossdomain[1024 * 16];
  size_t crossdomain_len;

public:
  FlvPlayerConfig();
  virtual ~FlvPlayerConfig();
  FlvPlayerConfig& operator=(const FlvPlayerConfig& rhv);
  virtual void set_default_config();
  virtual bool load_config(xmlnode* xml_config);
  virtual bool reload() const;
  virtual const char* module_name() const;
  virtual void dump_config() const;

private:
  bool load_config_unreloadable(xmlnode* xml_config);
  bool load_config_reloadable(xmlnode* xml_config);
  bool load_crossdomain_config();
  bool resove_config();
  bool parse_crossdomain_config();
};

#endif
