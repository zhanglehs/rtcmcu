#include "config/FlvPlayerConfig.h"

#include "target.h"
#include "config/TargetConfig.h"
#include "../util/log.h"
#include "../util/util.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

FlvPlayerConfig::FlvPlayerConfig()
{
  inited = false;
  set_default_config();
}

FlvPlayerConfig::~FlvPlayerConfig()
{
}

FlvPlayerConfig& FlvPlayerConfig::operator=(const FlvPlayerConfig& rhv)
{
  memmove(this, &rhv, sizeof(FlvPlayerConfig));
  return *this;
}

void FlvPlayerConfig::set_default_config()
{
  player_listen_ip[0] = 0;
  memset(player_listen_ip, 0, sizeof(player_listen_ip));
  player_listen_port = 10001;
  timeout_second = 60;
  stuck_long_duration = 5 * 60;
  max_players = 8000;
  always_generate_hls = false;
  ts_target_duration = 8;
  ts_segment_cnt = 4;
  buffer_max = 2 * 1024 * 1024;
  sock_snd_buf_size = 32 * 1024;
  normal_offset = 0;
  latency_offset = 3;
  memset(private_key, 0, sizeof(private_key));
  memset(super_key, 0, sizeof(super_key));
  memset(crossdomain, 0, sizeof(crossdomain));
  crossdomain_len = 0;
}

bool FlvPlayerConfig::load_config(xmlnode* xml_config)
{
  ASSERTR(xml_config != NULL, false);
  xmlnode *p = xmlgetchild(xml_config, module_name(), 0);
  ASSERTR(p != NULL, false);

  return load_config_unreloadable(p) && load_config_reloadable(p) && resove_config();
}

bool FlvPlayerConfig::reload() const
{
  return true;
}

const char* FlvPlayerConfig::module_name() const
{
  return "flv_player";
}

void FlvPlayerConfig::dump_config() const
{
  INF("player config: "
    "player_listen_ip=%s, player_listen_port=%u, "
    "stuck_long_duration=%d, timeout_second=%d, max_players=%d, buffer_max=%d, "
    "always_generate_hls=%d, ts_target_duration=%u, ts_segment_cnt=%u, "
    "private_key=%s, super_key=%s, sock_snd_buf_size=%d, normal_offset=%d, latency_offset=%d, "
    "crossdomain_path=%s, crossdomain_len=%lu",
    player_listen_ip, uint32_t(player_listen_port),
    stuck_long_duration, timeout_second, max_players, (int)buffer_max,
    int(always_generate_hls), ts_target_duration, ts_segment_cnt,
    private_key, super_key, sock_snd_buf_size, normal_offset, latency_offset,
    crossdomain_path.c_str(), crossdomain_len);
}

bool FlvPlayerConfig::load_config_unreloadable(xmlnode* xml_config)
{
  ASSERTR(xml_config != NULL, false);
  if (inited)
    return true;

  char *q = NULL;
  int tm = 0;

  q = xmlgetattr(xml_config, "listen_host");
  if (NULL != q)
  {
    strncpy(player_listen_ip, q, 31);
  }

  q = xmlgetattr(xml_config, "listen_port");
  if (!q)
  {
    fprintf(stderr, "player_listen_port get failed.\n");
    return false;
  }
  player_listen_port = atoi(q);
  if (player_listen_port <= 0)
  {
    fprintf(stderr, "player_listen_port not valid.\n");
    return false;
  }

  q = xmlgetattr(xml_config, "socket_send_buffer_size");
  if (!q)
  {
    fprintf(stderr, "socket_send_buffer_size get failed.\n");
    return false;
  }
  tm = atoi(q);
  if (tm <= 6 * 1024 || tm > 512 * 1024 * 1024)
  {
    fprintf(stderr, "socket_send_buffer_size not valid. value = %d\n", tm);
    return false;
  }
  sock_snd_buf_size = tm;

  q = xmlgetattr(xml_config, "buffer_max");
  if (!q)
  {
    fprintf(stderr, "player buffer_max get failed.\n");
    return false;
  }
  tm = atoi(q);
  if (tm <= 0 || tm >= 20 * 1024 * 1024)
  {
    fprintf(stderr, "player buffer_max not valid. value = %d\n", tm);
    return false;
  }
  buffer_max = tm;

  q = xmlgetattr(xml_config, "max_players");
  if (!q)
  {
    fprintf(stderr, "max_players get failed.\n");
    return false;
  }
  max_players = atoi(q);
  if (max_players <= 0)
  {
    fprintf(stderr, "max_players not valid.\n");
    return false;
  }

  q = xmlgetattr(xml_config, "always_generate_hls");
  if (!q)
  {
    fprintf(stderr, "parse always_generate_hls failed.");
    return false;
  }
  if (strcasecmp(q, "true") == 0)
    always_generate_hls = true;
  else
    always_generate_hls = false;

  q = xmlgetattr(xml_config, "ts_target_duration");
  if (!q || !util_check_digit(q, strlen(q)))
  {
    fprintf(stderr, "ts_target_duration get failed.\n");
    return false;
  }
  ts_target_duration = atoi(q);
  if (ts_target_duration <= 0)
  {
    fprintf(stderr, "ts_target_duration not valid.\n");
    return false;
  }

  q = xmlgetattr(xml_config, "ts_segment_cnt");
  if (!q || !util_check_digit(q, strlen(q)))
  {
    fprintf(stderr, "ts_segment_cnt get failed.\n");
    return false;
  }
  ts_segment_cnt = atoi(q);
  if (ts_segment_cnt <= 0)
  {
    fprintf(stderr, "ts_segment_cnt not valid.\n");
    return false;
  }

  inited = true;
  return true;
}

bool FlvPlayerConfig::load_config_reloadable(xmlnode* xml_config)
{
  ASSERTR(xml_config != NULL, false);
  char *q = NULL;

  q = xmlgetattr(xml_config, "private_key");
  if (!q)
  {
    fprintf(stderr, "player private_key get failed.\n");
    return false;
  }
  strncpy(private_key, q, sizeof(private_key)-1);

  q = xmlgetattr(xml_config, "super_key");
  if (!q)
  {
    fprintf(stderr, "player super_key get failed.\n");
    return false;
  }
  strncpy(super_key, q, sizeof(super_key)-1);

  q = xmlgetattr(xml_config, "timeout_second");
  if (!q)
  {
    fprintf(stderr, "timeout_second get failed.\n");
    return false;
  }
  timeout_second = atoi(q);
  if (timeout_second <= 0)
  {
    fprintf(stderr, "timeout_second not valid.\n");
    return false;
  }

  q = xmlgetattr(xml_config, "normal_offset");
  if (!q)
  {
    fprintf(stderr, "normal_offset get failed.\n");
    return false;
  }
  normal_offset = atoi(q);
  if (normal_offset < 0)
  {
    fprintf(stderr, "normal_offset %d not valid.\n", normal_offset);
    return false;
  }

  q = xmlgetattr(xml_config, "latency_offset");
  if (!q)
  {
    fprintf(stderr, "latency_offset get failed.\n");
    return false;
  }
  latency_offset = atoi(q);
  if (latency_offset < 0)
  {
    fprintf(stderr, "latency_offset %d not valid.\n", latency_offset);
    return false;
  }

  q = xmlgetattr(xml_config, "stuck_long_duration");
  if (!q)
  {
    fprintf(stderr, "stuck_long_duration get failed.\n");
    return false;
  }
  stuck_long_duration = atoi(q);
  if (stuck_long_duration < 0)
  {
    fprintf(stderr, "stuck_long_duration %d not valid.\n", stuck_long_duration);
    return false;
  }

  q = xmlgetattr(xml_config, "crossdomain_path");
  if (!q)
  {
    fprintf(stderr, "crossdomain_path get failed.\n");
    return false;
  }

  if (q[0] == '/')
  {
    crossdomain_path = q;
  }
  else
  {
    TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");
    //common_config->config_file.dir;
    crossdomain_path = common_config->config_file.dir + std::string(q);
  }

  if (!parse_crossdomain_config())
  {
    fprintf(stderr, "parse_crossdomain_config failed.\n");
    return false;
  }

  return true;
}

bool FlvPlayerConfig::resove_config()
{
  char ip[32] = { '\0' };
  int ret;

  if (strlen(player_listen_ip) == 0)
  {
    if (g_public_cnt < 1)
    {
      fprintf(stderr, "player listen ip not in conf and no public ip found\n");
      return false;
    }
    strncpy(player_listen_ip, g_public_ip[0], sizeof(player_listen_ip)-1);
  }

  strncpy(ip, player_listen_ip, sizeof(ip)-1);
  ret = util_interface2ip(ip, player_listen_ip, sizeof(player_listen_ip)-1);
  if (0 != ret)
  {
    fprintf(stderr, "player_listen_host resolv failed. host = %s, ret = %d\n", player_listen_ip, ret);
    return false;
  }

  return true;
}

bool FlvPlayerConfig::parse_crossdomain_config()
{
  int f = open(crossdomain_path.c_str(), O_RDONLY);
  if (-1 == f)
  {
    fprintf(stderr, "crossdomain file not valid. path = %s, error = %s \n", crossdomain_path.c_str(), strerror(errno));
    return false;
  }

  ssize_t total = 0;
  ssize_t n = 0;
  ssize_t max = sizeof(crossdomain);
  while (total < (int)(max - 1))
  {
    n = read(f, crossdomain + total, max - total);
    if (-1 == n)
    {
      fprintf(stderr, "crossdomain read failed. error = %s\n", strerror(errno));
      goto fail;
    }
    if (0 == n)
    {
      goto success;
    }
    if (n + total >= (int)max)
    {
      fprintf(stderr, "crossdomain too long. max = %zu\n", max - 1);
      goto fail;
    }
    total += n;
  }
fail:
  close(f);
  return false;

success:
  close(f);
  crossdomain[max - 1] = 0;
  crossdomain_len = strlen(crossdomain);
  return true;
}
