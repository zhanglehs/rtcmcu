#include "config/ConfigManager.h"

#include "config/TargetConfig.h"
#include "config/FlvPlayerConfig.h"
#include "config/RtpPlayerConfig.h"
#include "config/RtpUploaderConfig.h"
#include "config/RtpRelayConfig.h"
#include "config/CacheManagerConfig.h"
#include "config/HttpServerConfig.h"
#include "config/FlvPublishConfig.h"
#include "../util/log.h"
#include <algorithm>
#include <unistd.h>

#ifndef nullptr
#define nullptr NULL
#endif

static ConfigManager s_inst;

ConfigManager& ConfigManager::get_inst() {
  return s_inst;
}

ConfigModule* ConfigManager::get_inst_config_module(const char* name) {
  return s_inst.get_config_module(name);
}

ConfigManager::ConfigManager() {
  m_target_conf = new TargetConfig;
  m_flv_player_config = new FlvPlayerConfig;
  m_rtp_player_config = new RTPPlayerConfig;
  m_rtp_uploader_config = new RTPUploaderConfig;
  m_rtp_relay_config = new RtpRelayConfig;
  m_cache_manager_config = new media_manager::CacheManagerConfig;
  m_http_server_config = new HTTPServerConfig;
  m_publisher_config = new ModPublisherConfig;
}

ConfigManager::~ConfigManager() {
  delete m_target_conf;
  delete m_flv_player_config;
  delete m_rtp_player_config;
  delete m_rtp_uploader_config;
  delete m_rtp_relay_config;
  delete m_cache_manager_config;
  delete m_http_server_config;
  delete m_publisher_config;
}

int ConfigManager::Init(const char *file) {
  set_config_module(m_target_conf);
  set_config_module(m_flv_player_config);
  set_config_module(m_rtp_player_config);
  set_config_module(m_rtp_uploader_config);
  set_config_module(m_rtp_relay_config);
  set_config_module(m_cache_manager_config);
  set_config_module(m_http_server_config);
  set_config_module(m_publisher_config);
  set_default_config();

  m_config_file = file;
  return ParseConfigFile();
}

int ConfigManager::ParseConfigFile() {
  const char *file = m_config_file.c_str();

  char config_full_path[PATH_MAX];
  if (NULL == realpath(file, config_full_path)) {
    fprintf(stderr, "resolve config file path failed. \n");
    return -1;
  }

  TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");
  common_config->config_file.load_full_path(config_full_path);

  xmlnode* mainnode = xmlloadfile(file);
  if (mainnode == NULL) {
    fprintf(stderr, "mainnode get failed. \n");
    return -1;
  }
  struct xmlnode *root = xmlgetchild(mainnode, "interlive", 0);
  if (root == NULL) {
    fprintf(stderr, "node interlive get failed. \n");
    freexml(mainnode);
    return -1;
  }

  if (load_config(root)) {
    freexml(mainnode);
    return 0;
  }
  else {
    fprintf(stderr, "parse_config_file failed.\n");
    freexml(mainnode);
    return -1;
  }

  return 0;
}

void ConfigManager::set_default_config() {
  for (auto it = m_config_modules.begin();
    it != m_config_modules.end(); ++it) {
    ConfigModule* cm = it->second;
    cm->set_default_config();
  }
}

bool ConfigManager::load_config(xmlnode* xml_config) {
  for (auto it = m_config_modules.begin();
    it != m_config_modules.end(); ++it) {
    ConfigModule* cm = it->second;
    if (!(cm->load_config(xml_config))) {
      fprintf(stderr, "load %s config failed.\n", cm->module_name());
      return false;
    }
  }
  return true;
}

bool ConfigManager::reload() const {
  for (auto it = m_config_modules.begin();
    it != m_config_modules.end(); ++it) {
    ConfigModule* cm = it->second;
    if (!(cm->reload())) {
      return false;
    }
  }
  return true;
}

bool ConfigManager::reload(const char* name) const {
  auto it = m_config_modules.find(std::string(name));
  if (it == m_config_modules.end()) {
    return false;
  }

  return it->second->reload();
}

ConfigModule* ConfigManager::get_config_module(const char* name) {
  auto it = m_config_modules.find(std::string(name));
  if (it == m_config_modules.end()) {
    return nullptr;
  }
  return it->second;
}

ConfigModule* ConfigManager::set_config_module(ConfigModule* config_module) {
  ASSERTR(config_module != nullptr, false);
  ASSERTR(config_module->module_name() != nullptr, false);
  std::string name(config_module->module_name());

  auto it = m_config_modules.find(name);
  if (it == m_config_modules.end()) {
    m_config_modules[name] = config_module;
    return nullptr;
  }
  else {
    ConfigModule* old_cm = it->second;
    it->second = config_module;
    return old_cm;
  }
}

void ConfigManager::dump_config() const {
  for (auto it = m_config_modules.begin();
    it != m_config_modules.end(); ++it) {
    ConfigModule* cm = it->second;
    cm->dump_config();
  }
}
