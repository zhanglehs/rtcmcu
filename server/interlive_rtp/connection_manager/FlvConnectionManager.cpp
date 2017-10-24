/**********************************************************
 * YueHonghui, 2013-05-02
 * hhyue@tudou.com
 * copyright:youku.com
 * ********************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "connection_manager/FlvConnectionManager.h"
#include "target.h"
#include "../util/buffer.hpp"
#include "../util/util.h"
#include "../util/flv.h"
#include "../util/log.h"
#include "hls.h"
#include "connection_manager/flv_live_player.h"
#include "media_manager/cache_manager.h"
#include "config/FlvPlayerConfig.h"
#include "../util/access.h"

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
//#include "../target_config.h"

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

int LiveConnectionManager::Init(struct event_base *ev_base) {
  m_ev_base = ev_base;
  m_config = (FlvPlayerConfig *)ConfigManager::get_inst_config_module("flv_player");;

  int fd_socket = util_create_listen_fd("0.0.0.0", m_config->player_listen_port, 128);
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

bool LiveConnectionManager::HasPlayer(const StreamId_Ext &streamid) {
  return m_stream_groups.find(streamid.get_32bit_stream_id()) != m_stream_groups.end();
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
