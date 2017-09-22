/*
 * backend part of forward
 * author: hechao@youku.com
 * create: 2013-05-09
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <event.h>
#include <string>
#include <algorithm>

#include "module_backend.h"
#include "target.h"
//#include "http_server.h"
#include "forward_common.h"
#include "util/util.h"
#include "util/log.h"
#include "util/report.h"
#include "forward_client_rtp_tcp.h"
#include "module_tracker.h"
#include "../util/access.h"
#include "../target_config.h"


#include "event_loop.h"
#include "ip_endpoint.h"
#include "cmd_server.h"
#include "forward_server.h"
using namespace lcdn;
using namespace lcdn::net;
using namespace lcdn::base;
using namespace interlive;
using namespace interlive::forward_server;
extern EventLoop* tcp_event_loop;
static CMDServer* cmd_server = NULL;

void forward_server_init(struct event_base* mainbase, const backend_config* backend_conf) {
  if ((NULL == mainbase) || (NULL == backend_conf)) {
    WRN("forward_server_init:mainbase or backend_conf is null!");
    return;
  }
  tcp_event_loop = new EventLoop(mainbase);

  ForwardServer* fs = ForwardServer::get_server();
  if (fs == NULL) {
    delete cmd_server;
    cmd_server = NULL;
    return;
  }

  fs->create(*backend_conf);
}

void forward_server_stop() {
}

int backend_init(struct event_base *mainbase, struct session_manager *smng,
  const backend_config * backend_conf) {
  if (!mainbase || !backend_conf)
    return -1;

  TRC("backend_init");
  set_event_base(mainbase);
  forward_server_init(mainbase, backend_conf);

  TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");

  if (common_config->enable_rtp) {
    ForwardClientRtpTCPMgr *fcrtptcp = ForwardClientRtpTCPMgr::Instance();
    if (fcrtptcp)
    {
      fcrtptcp->set_main_base(mainbase);
      //fcrtptcp->init();
    }
  }

  g_forward_stat.stream_count = 0;

  return 0;
}

void backend_fini() {
}

void forward_server_on_second(time_t t) {
}

void backend_on_second(time_t t) {
  if (t % 30 == 0) {
    REPORT("FORWARD", "%hu\t%hu %hu %hu %hu", (uint16_t)FORWARD_STAT,
      g_forward_stat.stream_count, g_forward_stat.stream_alive_count,
      g_forward_stat.fc_conn_count, g_forward_stat.fs_conn_count);
  }
  ForwardServer::get_server()->on_second(t);
}

uint32_t backend_server_online_cnt(uint32_t streamid) {
  return 0;
}

void forward_server_on_millsecond(time_t t) {
}

void backend_on_millsecond(time_t t) {
  forward_server_on_millsecond(t);

  //TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");
  //if (common_config->enable_rtp) {
  //  ForwardClientRtpTCPMgr::get_inst()->onTimer();
  //}
}

void backend_del_stream_from_tracker_v3(StreamId_Ext stream_id, int level) {
  ForwardServer::get_server()->del_stream_from_tracker(stream_id, level);
}

int32_t backend_start_stream_rtp(const StreamId_Ext& stream_id) {
  TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");
  if (common_config->enable_rtp) {
    ForwardClientRtpTCPMgr *fcrtptcp = ForwardClientRtpTCPMgr::Instance();
    fcrtptcp->startStream(stream_id);
  }

  return -1;
}

int32_t backend_stop_stream_rtp(const StreamId_Ext& stream_id) {
  //#todo fsrtp stop stream
  TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");
  if (common_config->enable_rtp) {
    ForwardClientRtpTCPMgr *fcrtptcp = ForwardClientRtpTCPMgr::Instance();
    fcrtptcp->stopStream(stream_id);
  }
  return -1;
}

backend_config::backend_config()
{
  inited = false;
  set_default_config();
}

backend_config::~backend_config()
{
}

backend_config& backend_config::operator=(const backend_config& rhv)
{
  memmove(this, &rhv, sizeof(backend_config));
  return *this;
}

void backend_config::set_default_config()
{
  memset(backend_listen_ip, 0, sizeof(backend_listen_ip));
  backend_listen_port = 1840;
  backend_asn = 1;
  backend_region = 11;
  backend_level = 0;
  stream_timeout = 5;
  forward_client_capability = DEF_FC_CAPABILITY_DEFAULT;
  forward_client_receive_buf_max_len = DEF_FC_RECEIVE_LEN_DEFAULT;
  forward_server_max_session_num = FORWARD_MAX_SESSION_NUM_DEFAULT;
  fs_big_session_num = FSS_BIG_SESSION_COUNT_DEF;
  fs_big_session_size = FSS_BIG_SESSION_COUNT_DEF;
  fs_connection_buffer_size = FSS_CONNECTION_BUFFER_SIZE_DEF;
}

bool backend_config::load_config(xmlnode* xml_config)
{
  ASSERTR(xml_config != nullptr, false);
  xmlnode *p = xmlgetchild(xml_config, "backend", 0);
  ASSERTR(p != nullptr, false);

  return load_config_unreloadable(p) && load_config_reloadable(p) && resove_config();
}

bool backend_config::reload() const
{
  return true;
}

const char* backend_config::module_name() const
{
  return "backend";
}

void backend_config::dump_config() const
{
  INF("backend config: "
    "backend_listen_ip=%s, backend_listen_port=%hu, backend_asn=%hu, backend_region=%hu, backend_level=%hu, "
    "stream_timeout=%hu, "
    "forward_client_capability=%u, forward_client_receive_buf_max_len=%u, "
    "forward_server_max_session_num=%u, fs_big_session_num=%u, fs_big_session_size=%u"
    "fs_connection_buffer_size=%u",
    backend_listen_ip, backend_listen_port, backend_asn, backend_region, backend_level,
    stream_timeout,
    forward_client_capability, forward_client_receive_buf_max_len,
    forward_server_max_session_num, fs_big_session_num, fs_big_session_size,
    fs_connection_buffer_size);
}

bool backend_config::load_config_unreloadable(xmlnode* xml_config) {
  if (inited)
    return true;

  char *q = NULL;

  q = xmlgetattr(xml_config, "backend_listen_ip");
  if (q)
  {
    strncpy(backend_listen_ip, q, 31);
  }

  q = xmlgetattr(xml_config, "backend_listen_port");
  if (!q)
  {
    fprintf(stderr, "backend_listen_port get failed.\n");
    return false;
  }
  backend_listen_port = atoi(q);
  if (backend_listen_port <= 0)
  {
    fprintf(stderr, "backend_listen_port not valid.\n");
    return false;
  }

  q = xmlgetattr(xml_config, "backend_asn");
  if (!q)
  {
    fprintf(stderr, "backend_asn get failed.\n");
    return false;
  }
  backend_asn = atoi(q);

  q = xmlgetattr(xml_config, "backend_region");
  if (!q)
  {
    fprintf(stderr, "backend_region get failed.\n");
    return false;
  }
  backend_region = atoi(q);

  q = xmlgetattr(xml_config, "backend_level");
  if (!q)
  {
    fprintf(stderr, "backend_level get failed.\n");
    return false;
  }
  backend_level = atoi(q);

  q = xmlgetattr(xml_config, "stream_timeout");
  if (!q)
  {
    fprintf(stderr, "stream_timeout get failed.\n");
    return false;
  }
  stream_timeout = atoi(q);

  q = xmlgetattr(xml_config, "forward_client_capability");
  if (!q)
  {
    forward_client_capability = DEF_FC_CAPABILITY_DEFAULT;
    fprintf(stderr, "backend forward_client_capability not valid..use default value: %u\n", DEF_FC_CAPABILITY_DEFAULT);
  }
  else
  {
    forward_client_capability = atoi(q);
    if (forward_client_capability <= 0)
    {
      fprintf(stderr, "backend forward_client_capability not valid.use default value:%u \n", DEF_FC_CAPABILITY_DEFAULT);
      forward_client_capability = DEF_FC_CAPABILITY_DEFAULT;
    }
    else if (forward_client_capability > DEF_FC_CAPABILITY_MAX)
    {
      forward_client_capability = DEF_FC_CAPABILITY_MAX;
    }
  }

  forward_client_receive_buf_max_len = DEF_FC_RECEIVE_LEN_DEFAULT;
  q = xmlgetattr(xml_config, "forward_client_receive_buf_max_len");
  if (!q)
  {
    fprintf(stderr, "backend forward_client_receive_buf_max_len not valid.use default value: %u\n", DEF_FC_RECEIVE_LEN_DEFAULT);
  }
  else
  {
    forward_client_receive_buf_max_len = atoi(q);
    if (forward_client_receive_buf_max_len <= 0)
    {
      forward_client_receive_buf_max_len = DEF_FC_RECEIVE_LEN_DEFAULT;
      fprintf(stderr, "backend forward_client_receive_buf_max_len not valid.use default value: %u\n", DEF_FC_RECEIVE_LEN_DEFAULT);
    }
    else if (forward_client_receive_buf_max_len >= DEF_FC_RECEIVE_LEN_MAX)
    {
      forward_client_receive_buf_max_len = DEF_FC_RECEIVE_LEN_MAX;
    }
  }

  forward_server_max_session_num = FORWARD_MAX_SESSION_NUM_DEFAULT;
  q = xmlgetattr(xml_config, "forward_server_max_session_num");
  if (!q)
  {
    fprintf(stderr, "backend forward_server_max_session_num not valid.use default value: %u\n", FORWARD_MAX_SESSION_NUM_DEFAULT);
  }
  else
  {
    forward_server_max_session_num = atoi(q);
    if (forward_server_max_session_num <= 0)
    {
      forward_server_max_session_num = FORWARD_MAX_SESSION_NUM_DEFAULT;
      fprintf(stderr, "backend forward_server_max_session_num not valid.use default value: %u\n", FORWARD_MAX_SESSION_NUM_DEFAULT);
    }
    else if (forward_server_max_session_num >= FORWARD_MAX_SESSION_NUM_DEFAULT)
    {
      forward_server_max_session_num = FORWARD_MAX_SESSION_NUM_DEFAULT;
    }
  }

  fs_big_session_num = FSS_BIG_SESSION_COUNT_DEF;
  q = xmlgetattr(xml_config, "fs_big_session_num");
  if (!q)
  {
    fprintf(stderr, "backend fs_big_session_num not valid.use default value: %u\n", FSS_BIG_SESSION_COUNT_DEF);
  }
  else
  {
    fs_big_session_num = atoi(q);
    if (fs_big_session_num <= 0)
    {
      fs_big_session_num = FSS_BIG_SESSION_COUNT_DEF;
      fprintf(stderr, "backend fs_big_session_num not valid.use default value: %u\n", FSS_BIG_SESSION_COUNT_DEF);
    }
    else if (fs_big_session_num >= FSS_BIG_SESSION_COUNT_DEF)
    {
      fs_big_session_num = FSS_BIG_SESSION_COUNT_DEF;
    }
  }

  fs_big_session_size = FSS_BIG_SESSION_SIZE_DEF;
  q = xmlgetattr(xml_config, "fs_big_session_size");
  if (!q)
  {
    fprintf(stderr, "backend fs_big_session_size not valid.use default value: %u\n", FSS_BIG_SESSION_SIZE_DEF);
  }
  else
  {
    fs_big_session_size = atoi(q);
    if (fs_big_session_size <= 0)
    {
      fs_big_session_size = FSS_BIG_SESSION_SIZE_DEF;
      fprintf(stderr, "backend fs_big_session_size not valid.use default value: %u\n", FSS_BIG_SESSION_SIZE_DEF);
    }
    else if (fs_big_session_size >= FSS_BIG_SESSION_SIZE_DEF)
    {
      fs_big_session_size = FSS_BIG_SESSION_SIZE_DEF;
    }
  }


  fs_connection_buffer_size = FSS_CONNECTION_BUFFER_SIZE_DEF;
  q = xmlgetattr(xml_config, "fs_connection_buffer_size");
  if (!q)
  {
    fprintf(stderr, "backend fs_connection_buffer_size not valid.use default value: %u\n", FSS_CONNECTION_BUFFER_SIZE_DEF);
  }
  else
  {
    fs_connection_buffer_size = atoi(q);
    if (fs_connection_buffer_size <= 0)
    {
      fs_connection_buffer_size = FSS_CONNECTION_BUFFER_SIZE_DEF;
      fprintf(stderr, "backend fs_connection_buffer_size not valid.use default value: %u\n", FSS_CONNECTION_BUFFER_SIZE_DEF);
    }
    else if (fs_connection_buffer_size >= 2 * FSS_CONNECTION_BUFFER_SIZE_DEF)
    {
      fprintf(stderr, "backend fs_connection_buffer_size too big.use default value: %u\n", 2 * FSS_CONNECTION_BUFFER_SIZE_DEF);
      fs_connection_buffer_size = 2 * FSS_CONNECTION_BUFFER_SIZE_DEF;
    }
  }

  inited = true;
  return true;
}

bool backend_config::load_config_reloadable(xmlnode* xml_config)
{
  return true;
}

bool backend_config::resove_config()
{
  char ip[32] = { '\0' };
  int ret;

  const TargetConfig* common_config = (const TargetConfig*)ConfigManager::get_inst_config_module("common");
  ASSERTR(common_config != NULL, false);

  if (strlen(backend_listen_ip) == 0)
  {
    //if (strcmp("receiver", common_config->process_name) == 0)
    if (strstr(common_config->process_name, "receiver") != NULL)
    {
      if (g_private_cnt < 1)
      {
        fprintf(stderr, "backend listen ip not in conf and no private ip found\n");
        return false;
      }


      strncpy(backend_listen_ip, g_public_ip[0], sizeof(backend_listen_ip)-1);
    }
    //else if (strcmp("forward", common_config->process_name) == 0)
    else if (strstr(common_config->process_name, "forward") != NULL)
    {
      if (g_public_cnt < 1)
      {
        fprintf(stderr, "backend listen ip not in conf and no public ip found\n");
        return false;
      }

      strncpy(backend_listen_ip, g_public_ip[0], sizeof(backend_listen_ip)-1);
      //strncpy(backend_listen_ip, "103.41.143.109", sizeof(backend_listen_ip) - 1);
    }
    else
    {
      fprintf(stderr, "not supported PROCESS_NAME %s\n", PROCESS_NAME);
      return false;
    }
  }

  strncpy(ip, backend_listen_ip, sizeof(ip)-1);
  ret = util_interface2ip(ip, backend_listen_ip, sizeof(backend_listen_ip)-1);
  if (0 != ret)
  {
    fprintf(stderr, "backend listen ip resolv failed. host = %s, ret = %d\n", backend_listen_ip, ret);
    return false;
  }

  return true;
}
