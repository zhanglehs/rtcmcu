#include "config/TargetConfig.h"

#include "../util/access.h"
#include "../util/log.h"
#include <string.h>

int32_t FilePath::load_full_path(const char* input_path) {
  if (input_path == NULL || strlen(input_path) == 0) {
    return -1;
  }

  full_path = input_path;
  int len = full_path.length();
  int slash = full_path.rfind('/');
  dir = full_path.substr(0, slash + 1);
  file_name = full_path.substr(slash + 1, len - slash - 1);
  return 0;
}

//////////////////////////////////////////////////////////////////////////

TargetConfig::TargetConfig()
{
  inited = false;
  set_default_config();
}

TargetConfig::~TargetConfig()
{
}

TargetConfig& TargetConfig::operator=(const TargetConfig& rhv)
{
  memmove(this, &rhv, sizeof(TargetConfig));
  return *this;
}

void TargetConfig::set_default_config()
{
  // unreloadable
  media_buffer_size = 10;
  logger_mode = LOGGER_FILE;
  log_ctrl_port = 8080;
  memset(remote_logger_path, 0, sizeof(remote_logger_path));
  memset(logdir, 0, sizeof(logdir));
  memset(backtrace_dir, 0, sizeof(backtrace_dir));
  memset(record_dir, 0, sizeof(record_dir));
  memset(log_ctrl_ip, 0, sizeof(log_ctrl_ip));

  // reloadable
  log_level = LOG_LEVEL_INF;
  enable_uploader = 0;
  access_cut_size_MB = 300;
  log_cut_size_MB = 300;
  stream_idle_sec = 60;
  enable_idle_stream_collect = TRUE;
  enable_rtp = TRUE;
  enable_rtp2flv = TRUE;
  enable_flv2rtp = TRUE;
  jitter_buffer_time = 50;
  cpu_rate_threshold = 70;
  muxer_queue_threshold = 500;
  enable_check_broken_frame = TRUE;
  enable_use_nalu_split_code = TRUE;
}

bool TargetConfig::load_config(xmlnode* xml_config)
{
  ASSERTR(xml_config != NULL, false);
  xmlnode *p = xmlgetchild(xml_config, "common", 0);
  ASSERTR(p != NULL, false);

  return load_config_unreloadable(p) && load_config_reloadable(p);
}

bool TargetConfig::reload() const
{
  log_set_level(log_level);
  log_set_max_size(log_cut_size_MB * 1024 * 1024);
  access_set_max_size(access_cut_size_MB * 1024 * 1024);
  return true;
}

const char* TargetConfig::module_name() const
{
  return "common";
}

void TargetConfig::dump_config() const
{
  INF("common config: "
    "log_level=%d, enable_uploader=%d, logger_mode=%d, log_cut_size_MB=%u, access_cut_size_MB=%u, "
    "logdir=%s, remote_logger_path=%s, "
    "media_buffer_size=%u, stream_idle_sec=%u, enable_idle_stream_collect=%d, "
    "record_dir=%s, backtrace_dir=%s",
    log_level, enable_uploader, logger_mode, log_cut_size_MB, access_cut_size_MB,
    logdir, remote_logger_path,
    media_buffer_size, stream_idle_sec, int(enable_idle_stream_collect),
    record_dir, backtrace_dir);
}

bool TargetConfig::load_config_unreloadable(xmlnode* xml_config)
{
  ASSERTR(xml_config != NULL, false);

  if (inited)
    return true;

  const char *q = NULL;
  int tm = 0;

  q = xmlgetattr(xml_config, "media_buffer_size");
  if (!q)
  {
    fprintf(stderr, "media_buffer_size get failed.\n");
    return false;
  }
  tm = atoi(q);
  if (tm <= 0 || tm > 160)
  {
    fprintf(stderr, "media_buffer_size not valid. value = %d\n", tm);
    return false;
  }
  media_buffer_size = (uint32_t)tm;

  q = xmlgetattr(xml_config, "logger_mode");
  if (!q)
  {
    fprintf(stderr, "logger_mode get failed.\n");
    return false;
  }
  if (strncmp("FILE", q, 4) == 0)
  {
    logger_mode = LOGGER_FILE;
  }
  else if (strncmp("NET", q, 3) == 0)
  {
    logger_mode = LOGGER_NET;
  }
  else
  {
    fprintf(stderr, "logger_mode not valid. %s\n", q);
    return false;
  }

  q = xmlgetattr(xml_config, "remote_logger_path");
  if (!q)
  {
    fprintf(stderr, "remote_logger_path get failed.\n");
    return false;
  }
  strncpy(remote_logger_path, q, sizeof(remote_logger_path)-1);

  if (logger_mode == LOGGER_FILE)
  {
    q = xmlgetattr(xml_config, "logdir");
    if (!q)
    {
      fprintf(stderr, "logdir get failed.\n");
      return false;
    }

    strncpy(logdir, q, sizeof(logdir)-1);
    if (logdir[0] != '/')
    {
      char tempdir[PATH_MAX];
      sprintf(tempdir, "%s%s", config_file.dir.c_str(), logdir);
      strcpy(logdir, tempdir);
    }
  }

  q = xmlgetattr(xml_config, "backtrace_dir");
  if (!q || strlen(q) == 0)
  {
    backtrace_dir[0] = '.';
    backtrace_dir[1] = '\0';
  }
  else
  {
    strncpy(backtrace_dir, q, sizeof(backtrace_dir)-1);
  }

  q = xmlgetattr(xml_config, "record_dir");
  if (q && strlen(q) != 0)
  {
    strncpy(record_dir, q, sizeof(record_dir)-1);
  }

  q = xmlgetattr(xml_config, "enable_rtp");

  if (q)
  {
    if (strncmp("TRUE", q, 4) == 0)
      enable_rtp = TRUE;
    else
      enable_rtp = FALSE;
  }

  q = xmlgetattr(xml_config, "enable_rtp2flv");

  if (q)
  {
    if (strncmp("TRUE", q, 4) == 0)
      enable_rtp2flv = TRUE;
    else
      enable_rtp2flv = FALSE;
  }

  q = xmlgetattr(xml_config, "enable_flv2rtp");

  if (q)
  {
    if (strncmp("TRUE", q, 4) == 0)
      enable_flv2rtp = TRUE;
    else
      enable_flv2rtp = FALSE;
  }

  q = xmlgetattr(xml_config, "jitter_buffer_time");
  if (q) {
    jitter_buffer_time = atoi(q);
  }

  q = xmlgetattr(xml_config, "muxer_queue_threshold");
  if (q) {
    muxer_queue_threshold = atoi(q);
  }

  q = xmlgetattr(xml_config, "cpu_rate_threshold");
  if (q) {
    cpu_rate_threshold = atoi(q);
  }

  q = xmlgetattr(xml_config, "log_ctrl_ip");
  if (!q || strlen(q) == 0)
  {
    fprintf(stderr, "log_ctrl_ip empty");
    return false;
  }
  else
  {
    strncpy(log_ctrl_ip, q, sizeof(log_ctrl_ip)-1);
  }

  q = xmlgetattr(xml_config, "log_ctrl_port");
  if (q) {
    log_ctrl_port = atoi(q);
  }
  else {
    fprintf(stderr, "log_ctrl_port empty");
    return false;
  }

  inited = true;
  return true;
}

bool TargetConfig::load_config_reloadable(xmlnode* xml_config)
{
  const char *q = NULL;

  q = xmlgetattr(xml_config, "log_level");
  if (!q)
  {
    fprintf(stderr, "log_level get failed.\n");
    return false;
  }
  log_level = log_str2level(q);
  if (log_level <= 0)
  {
    fprintf(stderr, "log_level not valid.\n");
    return false;
  }

  q = xmlgetattr(xml_config, "enable_uploader");
  if (q)
  {
    enable_uploader = atoi(q);
  }

  q = xmlgetattr(xml_config, "access_cut_size_MB");
  if (!q)
  {
    fprintf(stderr, "access_cut_size_MB get failed.\n");
    return false;
  }
  access_cut_size_MB = atoi(q);
  if (access_cut_size_MB <= 0)
  {
    fprintf(stderr, "access_cut_size_MB not valid. %u\n", access_cut_size_MB);
    return false;
  }

  q = xmlgetattr(xml_config, "log_cut_size_MB");
  if (!q)
  {
    fprintf(stderr, "log_cut_size_MB get failed.\n");
    return false;
  }
  log_cut_size_MB = atoi(q);
  if (log_cut_size_MB <= 0)
  {
    fprintf(stderr, "log_cut_size_MB not valid. %u\n", log_cut_size_MB);
    return false;
  }

  q = xmlgetattr(xml_config, "stream_idle_sec");
  if (!q)
  {
    fprintf(stderr, "stream_idle_sec get failed.\n");
    return false;
  }
  stream_idle_sec = atoi(q);
  if (stream_idle_sec <= 0)
  {
    fprintf(stderr, "stream_idle_sec not valid. %u\n", stream_idle_sec);
    return false;
  }

  q = xmlgetattr(xml_config, "enable_idle_stream_collect");
  if (!q)
  {
    fprintf(stderr, "enable_idle_stream_collect get failed.\n");
    return false;
  }
  if (strncmp("TRUE", q, 4) == 0)
    enable_idle_stream_collect = TRUE;
  else
    enable_idle_stream_collect = FALSE;

  q = xmlgetattr(xml_config, "enable_check_broken_frame");
  if (q)
  {
    if (strncmp("TRUE", q, 4) == 0)
      enable_check_broken_frame = TRUE;
    else
      enable_check_broken_frame = FALSE;
  }

  q = xmlgetattr(xml_config, "enable_use_nalu_split_num");
  if (q)
  {
    if (strncmp("TRUE", q, 4) == 0)
      enable_use_nalu_split_code = TRUE;
    else
      enable_use_nalu_split_code = FALSE;
  }


  return true;
}
