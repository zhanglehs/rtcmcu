/**********************************************************
 * YueHonghui, 2013-05-02
 * hhyue@tudou.com
 * copyright:youku.com
 * ********************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "module_player.h"
#include "target.h"
#include "target_player.h"
#include "utils/memory.h"
#include "util/access.h"
#include "util/util.h"
#include "util/list.h"
#include "util/common.h"
#include "utils/buffer.hpp"
#include "util/flv.h"
#include "util/log.h"
#include "util/session.h"
#include "util/levent.h"
#include "util/report.h"
#include "testcase/delay_test.h"
#include "stream_manager.h"
#include "util/connection.h"
#include "hls.h"
#include "player/flv_live_player.h"

#include <assert.h>
#include <errno.h>
#include <event.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "../target_config.h"

//////////////////////////////////////////////////////////////////////////

player_config::player_config()
{
  inited = false;
  set_default_config();
}

player_config::~player_config()
{
}

player_config& player_config::operator=(const player_config& rhv)
{
  memmove(this, &rhv, sizeof(player_config));
  return *this;
}

void player_config::set_default_config()
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

bool player_config::load_config(xmlnode* xml_config)
{
  ASSERTR(xml_config != NULL, false);
  xmlnode *p = xmlgetchild(xml_config, "player", 0);
  ASSERTR(p != NULL, false);

  return load_config_unreloadable(p) && load_config_reloadable(p) && resove_config();
}

bool player_config::reload() const
{
  return true;
}

const char* player_config::module_name() const
{
  return "player";
}

void player_config::dump_config() const
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

bool player_config::load_config_unreloadable(xmlnode* xml_config)
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

bool player_config::load_config_reloadable(xmlnode* xml_config)
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
    crossdomain_path = common_config->config_file.dir + string(q);
  }

  if (!parse_crossdomain_config())
  {
    fprintf(stderr, "parse_crossdomain_config failed.\n");
    return false;
  }

  return true;
}

bool player_config::resove_config()
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

bool player_config::parse_crossdomain_config()
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

//////////////////////////////////////////////////////////////////////////

LiveConnection::LiveConnection() {
  memset(this, 0, sizeof(LiveConnection));
  ev_socket.fd = -1;
}

LiveConnection::~LiveConnection() {
  ev_socket.Stop();

  if (NULL != rb) {
    buffer_free(rb);
    rb = NULL;
  }

  if (NULL != wb) {
    buffer_free(wb);
    wb = NULL;
  }
}

void LiveConnection::EnableWrite() {
  ev_socket.EnableWrite();
}

void LiveConnection::DisableWrite() {
  ev_socket.DisableWrite();
}

void LiveConnection::Destroy(LiveConnection *c) {
  c->ev_socket.Stop();
  LiveConnectionManager::Instance()->OnConnectionClosed(c);
}

//////////////////////////////////////////////////////////////////////////

#define WRITE_MAX (128 * 1024)

LiveConnectionManager* LiveConnectionManager::m_inst = NULL;

LiveConnectionManager* LiveConnectionManager::Instance() {
  if (m_inst) {
    return m_inst;
  }
  m_inst = new LiveConnectionManager();
  return m_inst;
}

void LiveConnectionManager::DestroyInstance() {
  if (m_inst) {
    delete m_inst;
    m_inst = NULL;
  }
}

int LiveConnectionManager::Init(struct event_base *ev_base, const player_config * config) {
  m_ev_base = ev_base;
  m_config = config;

  int fd_socket = util_create_listen_fd("0.0.0.0", config->player_listen_port, 128);
  if (fd_socket < 0)	{
    ERR("create rtp tcp listen fd failed. ret = %d", fd_socket);
    return -1;
  }
  #ifdef TCP_DEFER_ACCEPT
    {
      int v = 30;             /* 30 seconds */
      if (-1 == setsockopt(fd_socket, IPPROTO_TCP, TCP_DEFER_ACCEPT, &v, sizeof(v))) {
        WRN("can't set TCP_DEFER_ACCEPT on player_listen_fd %d", fd_socket);
      }
    }
  #endif
  m_ev_socket.Start(ev_base, fd_socket, &LiveConnectionManager::OnSocketAccept, this);

  media_manager::FlvCacheManager *cache = media_manager::FlvCacheManager::Instance();
  cache->register_watcher(&LiveConnectionManager::OnRecvStreamData, media_manager::CACHE_WATCHING_ALL, this);
  return 0;
}

void LiveConnectionManager::OnConnectionClosed(LiveConnection *c) {
  auto it = m_stream_groups.find(c->streamid.get_32bit_stream_id());
  if (it != m_stream_groups.end()) {
    std::set<LiveConnection*> &connections = it->second;
    connections.erase(c);
    if (connections.empty()) {
      m_stream_groups.erase(it);
    }
  }
  m_connections.erase(c);
}

void LiveConnectionManager::OnSocketAccept(const int fd, const short which, void *arg) {
  LiveConnectionManager *pThis = (LiveConnectionManager*)arg;
  pThis->OnSocketAcceptImpl(fd, which);
}

void LiveConnectionManager::OnSocketAcceptImpl(const int fd, const short which) {
  if (which & EV_READ) {
    struct sockaddr_in remote;
    socklen_t len = sizeof(struct sockaddr_in);
    memset(&remote, 0, len);
    int newfd = accept(fd, (struct sockaddr *) &remote, &len);
    if (-1 == newfd) {
      //PLAYER_ERR("fd #%d accept() failed, error = %s", fd, strerror(errno));
      return;
    }

    util_set_cloexec(newfd, TRUE);
    int val = 1;
    if (-1 == setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&val, sizeof(val))) {
      close(newfd);
      return;
    }
    if (-1 == setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
      (void *)&m_config->sock_snd_buf_size, sizeof(m_config->sock_snd_buf_size))) {
      close(newfd);
      return;
    }

    LiveConnection *c = new LiveConnection();
    c->remote = remote;
    strcpy(c->remote_ip, inet_ntoa(c->remote.sin_addr));
    c->rb = buffer_create_max(16 * 1024, m_config->buffer_max);
    c->wb = buffer_create_max(512 * 1024, m_config->buffer_max);
    c->ev_socket.Start(m_ev_base, newfd, &LiveConnectionManager::OnSocketData, c);
    c->active_time_s = time(NULL);
    m_connections.insert(c);
  }
}

void LiveConnectionManager::OnSocketData(const int fd, const short which, void *arg) {
  LiveConnection *c = (LiveConnection*)arg;
  LiveConnectionManager::Instance()->OnSocketDataImpl(c, which);
}

static void HttpErrorResponse(LiveConnection *c, http_code code) {
  c->async_close = true;
  c->EnableWrite();

  const char *code_str = util_http_code2numstr(code);
  const char *code_msg = util_http_code2str(code);
  if (!code_str || !code_msg) {
    return;
  }
  ACCESS("player %s:%d %s", c->remote_ip, (int)c->remote.sin_port, code_str);

  char rsp_line[128];
  memset(rsp_line, 0, sizeof(rsp_line));
  snprintf(rsp_line, sizeof(rsp_line), "HTTP/1.0 %s %s\r\nConnection: close\r\n\r\n", code_str, code_msg);
  buffer_append_ptr(c->wb, rsp_line, strlen(rsp_line));
}

void LiveConnectionManager::OnSocketDataImpl(LiveConnection *c, const short which) {
  if (which & EV_READ) {
    if (c->live) {
      char buf[128];
      int len = read(c->ev_socket.fd, buf, 128);
      if ((len == -1 && errno != EINTR && errno != EAGAIN) || len == 0) {
        LiveConnection::Destroy(c);
      }
    }
    else {
      char method[32] = { 0 };
      char path[128] = { 0 };
      char query[1024] = { 0 };
      {
        // 一大堆http协议的处理代码，写得很丑陋
        const int max_http_header_len = 16 * 1024;
        buffer *rb = c->rb;
        int len = buffer_read_fd_max(rb, c->ev_socket.fd, max_http_header_len);
        if (len < 0) {
          LiveConnection::Destroy(c);
          return;
        }
        else if (len == 0) {
          // TODO: zhangle
          int i = 0;
          i++;
        }
        const char *buf = buffer_data_ptr(rb);
        len = (int)buffer_data_len(rb);
        const char *header_end = (const char *)memmem(buf, len, "\r\n\r\n", strlen("\r\n\r\n"));
        if (header_end == NULL) {
          if (len >= max_http_header_len) {
            //PLAYER_WRN("http req line too long");
            HttpErrorResponse(c, HTTP_414);
          }
          return;
        }
        int header_len = (header_end - buf) + strlen("\r\n\r\n");
        const char *first_line_end = (const char *)memmem(buf, header_len, "\r\n", strlen("\r\n"));
        int first_line_len = first_line_end - buf;
        int parse_req = util_http_parse_req_line(buf, first_line_len,
          method, sizeof(method),
          path, sizeof(path),
          query, sizeof(query));
        if (parse_req < 0) {
          WRN("parse_req_line failed. connection closing.");
          HttpErrorResponse(c, HTTP_400);
          return;
        }
      }

      if (strlen(method) != 3 || 0 != strncmp(method, "GET", 3)) {
        WRN("method not valid. connection closing. %s:%d, method = %s", c->remote_ip, (int)c->remote.sin_port, method);
        HttpErrorResponse(c, HTTP_404);
        return;
      }

      const char *CROSSDOMAIN = "/crossdomain.xml";
      if (strlen(CROSSDOMAIN) == strlen(path) && 0 == strncmp(path, CROSSDOMAIN, strlen(CROSSDOMAIN))) {
        c->live = new CrossdomainLivePlayer(c, m_config);
      }
      else if (0 == strncmp(path, "/live/nodelay/v1/", strlen("/live/nodelay/v1/"))) {
        if (query[0]) {
          c->live = new FlvLivePlayer(c);
          char buf[64];
          memset(buf, 0, sizeof(buf));
          if (!util_path_str_get(path, strlen(path), 4, buf, 64)
            || !util_check_hex(buf, 32) || (c->streamid.parse(buf, 16) != 0)) {
            LiveConnection::Destroy(c);
            return;
          }
          m_stream_groups[c->streamid.get_32bit_stream_id()].insert(c);
        }
      }

      if (c->live) {
        c->EnableWrite();
      }
      else {
        HttpErrorResponse(c, HTTP_404);
      }
    }
  }

  if (which & EV_WRITE) {
    if (c->live && !c->async_close) {
      c->live->OnWrite();
    }

    int write_len = buffer_write_fd_max(c->wb, c->ev_socket.fd, WRITE_MAX);
    if (-1 == write_len) {
      INF("crossdomain (%s:%d) disconnected...", c->remote_ip, (int)c->remote.sin_port);
      LiveConnection::Destroy(c);
      return;
    }
    else if (write_len > 0) {
      c->active_time_s = time(NULL);
    }

    if (0 == buffer_data_len(c->wb)) {
      c->DisableWrite();
      if (c->async_close) {
        LiveConnection::Destroy(c);
        return;
      }
    }
    buffer_try_adjust(c->wb);
  }
}

void LiveConnectionManager::OnRecvStreamData(StreamId_Ext streamid, uint8_t watch_type, void* arg) {
  LiveConnectionManager *pThis = (LiveConnectionManager*)arg;
  pThis->OnRecvStreamDataImpl(streamid, watch_type);
}

void LiveConnectionManager::OnRecvStreamDataImpl(StreamId_Ext streamid, uint8_t watch_type) {
  auto it1 = m_stream_groups.find(streamid.get_32bit_stream_id());
  if (it1 == m_stream_groups.end()) {
    return;
  }
  std::set<LiveConnection*> &connections = it1->second;
  for (auto it = connections.begin(); it != connections.end(); it++) {
    LiveConnection *c = *it;
    c->EnableWrite();
  }
}

void LiveConnectionManager::StartTimer() {
  struct timeval tv;
  evtimer_set(&m_ev_timer, OnTimer, (void *)this);
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  event_base_set(m_ev_base, &m_ev_timer);
  evtimer_add(&m_ev_timer, &tv);
}

void LiveConnectionManager::OnTimer(const int fd, short which, void *arg) {
  LiveConnectionManager *pThis = (LiveConnectionManager*)arg;
  pThis->OnTimerImpl();
}

void LiveConnectionManager::OnTimerImpl() {
  time_t now = time(NULL);
  std::set<LiveConnection*> trash;
  for (auto it = m_connections.begin(); it != m_connections.end(); it++) {
    if (now - (*it)->active_time_s > 30) {
      trash.insert(*it);
    }
  }
  for (auto it = trash.begin(); it != trash.end(); it++) {
    LiveConnection::Destroy(*it);
  }
  StartTimer();
}
