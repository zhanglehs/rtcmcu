#ifndef __TARGET_CONFIG_H
#define __TARGET_CONFIG_H

#include "config/ConfigManager.h"
#include <limits.h>
#include <string>

class FilePath {
public:
  int32_t load_full_path(const char* path);
  std::string full_path;
  std::string dir;
  std::string file_name;
};

class TargetConfig : public ConfigModule {
private:
  bool inited;

public:
  int log_level;
  short enable_uploader;
  uint32_t media_buffer_size;
  uint32_t access_cut_size_MB;
  uint32_t log_cut_size_MB;
  int logger_mode;
  uint32_t stream_idle_sec;
  char remote_logger_path[32];
  char logdir[PATH_MAX];
  char record_dir[PATH_MAX];
  char backtrace_dir[PATH_MAX];
  bool enable_idle_stream_collect;
  bool enable_rtp;
  bool enable_rtp2flv;
  bool enable_flv2rtp;
  bool enable_check_broken_frame;
  bool enable_use_nalu_split_code;
  uint32_t jitter_buffer_time;
  uint32_t muxer_queue_threshold;
  uint32_t cpu_rate_threshold;
  FilePath config_file;
  char log_ctrl_ip[PATH_MAX];
  uint16_t log_ctrl_port;

  TargetConfig();
  virtual ~TargetConfig();
  TargetConfig& operator=(const TargetConfig& rhv);

  virtual void set_default_config();
  virtual bool load_config(xmlnode* xml_config);
  virtual bool reload() const;
  virtual const char* module_name() const;
  virtual void dump_config() const;

private:
  bool load_config_unreloadable(xmlnode* xml_config);
  bool load_config_reloadable(xmlnode* xml_config);
};

#endif
