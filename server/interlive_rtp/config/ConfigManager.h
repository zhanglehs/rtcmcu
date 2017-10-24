#ifndef _CONFIG_MANAGER_H_
#define _CONFIG_MANAGER_H_

#include "../util/xml.h"
#include <map>
#include <string>

class ConfigModule {
public:
  ConfigModule() {}
  virtual ~ConfigModule() {}
  virtual void set_default_config() = 0;
  virtual bool load_config(xmlnode* xml_config) = 0;
  virtual bool reload() const { return true; }
  virtual const char* module_name() const = 0;
  virtual void dump_config() const = 0;
};

class TargetConfig;
class FlvPlayerConfig;
class RTPPlayerConfig;
class RTPUploaderConfig;
class RtpRelayConfig;
class HTTPServerConfig;
class ModPublisherConfig;
namespace media_manager {
  class CacheManagerConfig;
}

class ConfigManager {
public:
  static ConfigManager& get_inst();
  static ConfigModule* get_inst_config_module(const char* name);

  ConfigManager();
  ~ConfigManager();

  int Init(const char *file);
  int ParseConfigFile();
  void set_default_config();
  bool load_config(xmlnode* xml_config);
  bool reload() const;
  bool reload(const char* name) const;
  ConfigModule* get_config_module(const char* name);
  ConfigModule* set_config_module(ConfigModule* config_module);
  void dump_config() const;

private:
  std::map<std::string, ConfigModule*> m_config_modules;
  std::string m_config_file;

  TargetConfig *m_target_conf;
  FlvPlayerConfig *m_flv_player_config;
  RTPPlayerConfig *m_rtp_player_config;
  RTPUploaderConfig *m_rtp_uploader_config;
  RtpRelayConfig *m_rtp_relay_config;
  media_manager::CacheManagerConfig *m_cache_manager_config;
  HTTPServerConfig *m_http_server_config;
  ModPublisherConfig *m_publisher_config;
};

#endif
