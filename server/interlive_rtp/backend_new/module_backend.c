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
#include "forward_common.h"
#include "util/util.h"
#include "util/log.h"
#include "util/report.h"
#include "forward_client_rtp_tcp.h"
#include "module_tracker.h"
#include "../util/access.h"
#include "../target_config.h"
#include "forward_server.h"

#define ENABLE_RTP_PULL
//#define ENABLE_RTP_PUSH

int backend_init(struct event_base *mainbase, const backend_config *backend_conf) {
  if (!mainbase || !backend_conf) {
    return -1;
  }

  TRC("backend_init");
  set_event_base(mainbase);
  interlive::forward_server::ForwardServer::get_server()->create(*backend_conf);

  g_forward_stat.stream_count = 0;

  return 0;
}

void backend_fini() {
}

void backend_del_stream_from_tracker_v3(const StreamId_Ext& stream_id, int level) {
  interlive::forward_server::ForwardServer::get_server()->del_stream_from_tracker(stream_id, level);
}

//////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////

RelayManager* RelayManager::m_inst = NULL;

RelayManager* RelayManager::Instance() {
  if (m_inst) {
    return m_inst;
  }
  m_inst = new RelayManager();
  return m_inst;
}

void RelayManager::DestroyInstance() {
  if (m_inst) {
    delete m_inst;
    m_inst = NULL;
  }
}

RelayManager::RelayManager() {
  m_ev_base = NULL;
  m_push_manager = new RtpPushTcpManager();
  m_pull_manager = new RtpPullTcpManager();
}

RelayManager::~RelayManager() {
  delete m_pull_manager;
  delete m_push_manager;
}

int RelayManager::Init(struct event_base *ev_base) {
  m_ev_base = ev_base;
  m_pull_manager->Init(ev_base);
  m_push_manager->Init(ev_base);
  return 0;
}

int RelayManager::StartPullRtp(const StreamId_Ext& stream_id) {
#ifdef ENABLE_RTP_PULL
  m_pull_manager->StartPull(stream_id);
#endif
  return 0;
}

int RelayManager::StopPullRtp(const StreamId_Ext& stream_id) {
  m_pull_manager->StopPull(stream_id);
  return 0;
}

int RelayManager::StartPushRtp(const StreamId_Ext& stream_id) {
#ifdef ENABLE_RTP_PUSH
  m_push_manager->StartPush(stream_id);
#endif
  return 0;
}

int RelayManager::StopPushRtp(const StreamId_Ext& stream_id) {
  m_push_manager->StopPush(stream_id);
  return 0;
}

//////////////////////////////////////////////////////////////////////////

#include "version.h"
#include "perf.h"

HttpServerManager* HttpServerManager::m_inst = NULL;

HttpServerManager* HttpServerManager::Instance() {
  if (m_inst) {
    return m_inst;
  }
  m_inst = new HttpServerManager();
  return m_inst;
}

void HttpServerManager::DestroyInstance() {
  if (m_inst) {
    delete m_inst;
    m_inst = NULL;
  }
}

HttpServerManager::HttpServerManager() {
  m_ev_base = NULL;
  m_ev_http = NULL;
}

HttpServerManager::~HttpServerManager() {
  if (m_ev_http) {
    evhttp_free(m_ev_http);
  }
}

int HttpServerManager::Init(struct event_base *ev_base, unsigned short port) {
  m_ev_base = ev_base;
  m_ev_http = evhttp_new(m_ev_base);
  int ret = evhttp_bind_socket(m_ev_http, "0.0.0.0", port);
  if (ret != 0) {
    return -1;
  }
  evhttp_set_timeout(m_ev_http, 10);
  evhttp_set_gencb(m_ev_http, DefaultHandler, NULL);
  return 0;
}

int HttpServerManager::AddHandler(const char *path, void(*cb)(struct evhttp_request *, void *), void *cb_arg) {
  return evhttp_set_cb(m_ev_http, path, cb, cb_arg);
}

void HttpServerManager::DefaultHandler(struct evhttp_request *requset, void *arg) {
  const struct evhttp_uri *uri = evhttp_request_get_evhttp_uri(requset);
  if (uri == NULL) {
    return;
  }
  //const char *query = evhttp_uri_get_query(uri);
  const char *path = evhttp_uri_get_path(uri);
  if (path == NULL) {
    return;
  }

  if (strcmp(path, "/get_version") == 0) {
    json_object* root = json_object_new_object();
    json_object_object_add(root, "version", json_object_new_string(VERSION));

    const char *content = json_object_to_json_string(root);
    struct evbuffer *buf = evbuffer_new();
    evbuffer_add(buf, content, strlen(content));
    evhttp_send_reply(requset, HTTP_OK, "OK", buf);
    evbuffer_free(buf);
    json_object_put(root);
  }
  else if (strcmp(path, "/get_config") == 0) {
  }
  else if (strcmp(path, "/get_server_info") == 0) {
    Perf * perf = Perf::get_instance();
    sys_info_t sys_info = perf->get_sys_info();
    json_object* root = json_object_new_object();
    json_object* j_online_processors = json_object_new_int(sys_info.online_processors);
    json_object* j_mem = json_object_new_int(sys_info.mem_ocy);
    json_object* j_free_mem = json_object_new_int(sys_info.free_mem_ocy);
    json_object* j_cpu_rate = json_object_new_int(sys_info.cpu_rate);
    json_object_object_add(root, "online_processors", j_online_processors);
    json_object_object_add(root, "total_memory", j_mem);
    json_object_object_add(root, "free_memory", j_free_mem);
    json_object_object_add(root, "cpu_rate", j_cpu_rate);

    const char *content = json_object_to_json_string(root);
    struct evbuffer *buf = evbuffer_new();
    evbuffer_add(buf, content, strlen(content));
    evhttp_send_reply(requset, HTTP_OK, "OK", buf);
    evbuffer_free(buf);
    json_object_put(root);
  }
}
