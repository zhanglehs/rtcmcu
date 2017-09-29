#include "uploader/RtpTcpConnectionManager.h"
#include "log.h"
#include "util/util.h"
#include "common/proto.h"
#include "common/proto_define.h"
#include "common/proto_rtp_rtcp.h"
#include "util/flv.h"
#include "utils/memory.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include "util/report.h"
#include "util/access.h"
#include "util/util.h"
#include "assert.h"
#include "target_uploader.h"
#include "cache_manager.h"
#include "stream_manager.h"
//#include "player/rtp_tcp_player.h"
//#include "uploader/rtp_tcp_uploader.h"
#include "target_config.h"
#include "perf.h"
#include "player/rtp_player_config.h"
#include "uploader/rtp_uploader_config.h"
#include "media_manager/rtp2flv_remuxer.h"
#include "media_manager/rtp_block_cache.h"

#define MAX_LEN_PER_READ (1024 * 128)

namespace {
  int bindUdpSocket(const char *local_ip, uint16_t local_port) {
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(local_ip);
    addr.sin_port = htons(local_port);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
      return -1;
    }

    fcntl(fd, F_SETFD, FD_CLOEXEC);

    int ret = util_set_nonblock(fd, TRUE);
    if (ret < 0) {
      close(fd);
      return -1;
    }

    int opt = 1;
    ret = setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &opt, sizeof(opt));
    if (ret == -1) {
      close(fd);
      return -1;
    }

    ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1) {
      close(fd);
      return -1;
    }
    return fd;
  }
}

//////////////////////////////////////////////////////////////////////////

RtpConnection::RtpConnection() {
  memset(this, 0, sizeof(RtpConnection));
  ev_socket.fd = -1;
}

RtpConnection::~RtpConnection() {
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

bool RtpConnection::IsPlayer() {
  return type == CONN_TYPE_PLAYER;
}

bool RtpConnection::IsUploader() {
  return type == CONN_TYPE_UPLOADER;
}

void RtpConnection::EnableWrite() {
  if (udp) {
    ((RtpUdpServerManager*)manager)->EnableWrite();
  }
  else {
    ev_socket.EnableWrite();
  }
}

void RtpConnection::DisableWrite() {
  if (udp) {
    ((RtpUdpServerManager*)manager)->DisableWrite();
  }
  else {
    ev_socket.DisableWrite();
  }
}

void RtpConnection::Destroy(RtpConnection *c) {
  if (!c->udp) {
    c->ev_socket.Stop();
  }
  c->manager->OnConnectionClosed(c);
}

//////////////////////////////////////////////////////////////////////////

int RtpManagerBase::SendRtcp(RtpConnection *c, const avformat::RtcpPacket *rtcp) {
  unsigned char buf[2048];

  unsigned char *rtcp_buf = buf + sizeof(proto_header)+STREAM_ID_LEN;
  int rtcp_max_len = sizeof(buf)-sizeof(proto_header)-STREAM_ID_LEN;
  size_t rtcp_len = 0;
  rtcp->Build(rtcp_buf, &rtcp_len, (size_t)rtcp_max_len);

  int total_len = sizeof(proto_header)+STREAM_ID_LEN + rtcp_len;
  uint32_t len = 0;
  encode_header_uint8(buf, len, CMD_RTCP_U2R_PACKET, total_len);
  memcpy(buf + sizeof(proto_header), &(c->streamid), STREAM_ID_LEN);

  SendData(c, buf, total_len);
  return 0;
}

int RtpManagerBase::SendRtp(RtpConnection *c, const unsigned char *rtp, int rtp_len, uint32_t ssrc) {
  unsigned char buf[2048];
  int total_len = sizeof(proto_header)+STREAM_ID_LEN + rtp_len;
  uint32_t len = 0;
  encode_header_uint8(buf, len, CMD_RTP_D2P_PACKET, total_len);
  memcpy(buf + sizeof(proto_header), &(c->streamid), STREAM_ID_LEN);

  unsigned char *rtp_buf = buf + sizeof(proto_header)+STREAM_ID_LEN;
  memcpy(rtp_buf, rtp, rtp_len);

  if (ssrc != 0) {
    // hack ssrc for player
    avformat::RTP_FIXED_HEADER* rtp_header = (avformat::RTP_FIXED_HEADER*)rtp_buf;
    rtp_header->set_ssrc(ssrc);
  }

  SendData(c, buf, total_len);
  return 0;
}

int RtpManagerBase::SendData(RtpConnection *c, const unsigned char *data, int len) {
  if (len <= 0) {
    return 0;
  }
  if (c->udp) {
    ((RtpUdpServerManager*)c->manager)->SendUdpData(c, data, len);
  }
  else {
    buffer_expand_capacity(c->wb, len);
    buffer_append_ptr(c->wb, data, len);
  }
  c->EnableWrite();
  return 0;
}

int RtpManagerBase::OnReadPacket(RtpConnection *c, buffer *buf) {
  if (buffer_data_len(buf) < sizeof(proto_header)) {
    if (c->udp) {
      return -1;
    }
    else {
      return 0;
    }
  }
  proto_header h;
  if (0 != decode_header(buf, &h)) {
    WRN("recv, decode packet header failed.");
    return -2;
  }
  if (h.size >= buffer_max(buf)) {
    WRN("recv, pakcet too len, size = %u, (%s:%d)",
      h.size, c->remote_ip, (int)c->remote.sin_port);
    return -3;
  }
  if (c->udp) {
    if (buffer_data_len(buf) != h.size) {
      return -4;
    }
  }
  else {
    if (buffer_data_len(buf) < h.size) {
      return 0;
    }
  }

  c->recv_bytes += h.size;
  c->recv_bytes_by_min += h.size;

  switch (h.cmd) {
  case CMD_RTP_U2R_REQ_STATE:
    c->type = RtpConnection::CONN_TYPE_UPLOADER;
    {
      rtp_u2r_req_state req;
      rtp_u2r_rsp_state rsp;
      if (0 != decode_rtp_u2r_req_state(&req, NULL, buf)) {
        return -5;
      }
      rsp.version = req.version;
      memmove(&rsp.streamid, &req.streamid, sizeof(req.streamid));

      c->streamid = req.streamid;

      RTPPlayerConfig *config = (RTPPlayerConfig *)ConfigManager::get_inst_config_module("rtp_uploader");
      if (0 != m_trans_mgr->_open_trans(c, &config->get_rtp_trans_config())) {
        return -6;
      }

      // TODO: zhangle, check streamid

      rsp.result = U2R_RESULT_SUCCESS;

      uint8_t rsp_buf[256];
      uint16_t rsp_len = sizeof(rsp_buf);
      if (0 != encode_rtp_u2r_rsp_state_uint8(&rsp, rsp_buf, rsp_len)) {
        return -7;
      }
      SendData(c, rsp_buf, rsp_len);
    }
    break;
  case CMD_RTP_D2P_REQ_STATE:
    c->type = RtpConnection::CONN_TYPE_PLAYER;
    {
      rtp_d2p_req_state req;
      rtp_d2p_rsp_state rsp;
      if (0 != decode_rtp_d2p_req_state(&req, NULL, buf)) {
        return -1;
      }
      rsp.version = req.version;
      memmove(&rsp.streamid, &req.streamid, sizeof(req.streamid));

      c->streamid = req.streamid;

      RTPPlayerConfig *config = (RTPPlayerConfig *)ConfigManager::get_inst_config_module("rtp_player");
      if (0 != m_trans_mgr->_open_trans(c, &config->get_rtp_trans_config())) {
        return -5;
      }

      rsp.result = U2R_RESULT_SUCCESS;

      uint8_t rsp_buf[256];
      uint16_t rsp_len = sizeof(rsp_buf);
      if (0 != encode_rtp_d2p_rsp_state_uint8(&rsp, rsp_buf, rsp_len)) {
        return -6;
      }
      SendData(c, rsp_buf, rsp_len);
    }
    break;
  case CMD_RTP_D2P_RSP_STATE:   // pull/push client
    break;
  case CMD_RTP_D2P_PACKET:      // pull client
  case CMD_RTP_U2R_PACKET:
    if (c->type == RtpConnection::CONN_TYPE_UNKOWN) {
      return -5;
    }
    {
      int head_len = sizeof(proto_header)+sizeof(rtp_u2r_packet_header);
      m_trans_mgr->OnRecvRtp(c, buffer_data_ptr(buf) + head_len, h.size - head_len);
    }
    break;
  case CMD_RTCP_U2R_PACKET:
  case CMD_RTCP_D2P_PACKET:
    if (c->type == RtpConnection::CONN_TYPE_UNKOWN) {
      return -5;
    }
    {
      int head_len = sizeof(proto_header)+sizeof(rtp_u2r_packet_header);
      m_trans_mgr->OnRecvRtcp(c, buffer_data_ptr(buf) + head_len, h.size - head_len);
    }
    break;
  default:
    return -6;
  }

  if (!c->udp) {
    buffer_ignore(buf, h.size);
  }
  return 1;
}

RtpManagerBase::RtpManagerBase() {
}

RtpManagerBase::~RtpManagerBase() {
}

RTPTransManager * RtpManagerBase::m_trans_mgr = NULL;
//RtpCacheManager * RtpManagerBase::m_rtp_cache = NULL;

//////////////////////////////////////////////////////////////////////////

RtpTcpManager::RtpTcpManager() {
  //memset(&m_ev_timer, 0, sizeof(m_ev_timer);
}

RtpTcpManager::~RtpTcpManager() {
}

RtpConnection* RtpTcpManager::CreateConnection(struct sockaddr_in *remote, int socket) {
  RtpConnection *c = new RtpConnection();
  c->udp = false;
  c->remote = *remote;
  strcpy(c->remote_ip, inet_ntoa(c->remote.sin_addr));
  c->rb = buffer_create_max(2048, 5 * 1024 * 1024/*m_config->buffer_max*/);
  c->wb = buffer_create_max(2048, 5 * 1024 * 1024/*m_config->buffer_max*/);
  c->ev_socket.Start(m_ev_base, socket, &RtpTcpManager::OnSocketData, c);
  c->manager = this;
  c->create_time = time(NULL);
  //c->set_active();
  //INF("uploader accepted. socket=%d, remote=%s:%d", c->fd_socket, c->remote_ip,
  //  (int)c->remote.sin_port);

  if (m_connections.empty()) {
    StartTimer();
  }
  m_connections.insert(c);
  return c;
}

void RtpTcpManager::OnConnectionClosed(RtpConnection *c) {
  m_connections.erase(c);
  if (m_connections.empty()) {
    evtimer_del(&m_ev_timer);
  }
  m_trans_mgr->_close_trans(c);
}

void RtpTcpManager::OnSocketData(const int fd, const short which, void *arg) {
  RtpConnection *c = (RtpConnection*)arg;
  ((RtpTcpManager*)(c->manager))->OnSocketDataImpl(c, which);
}

void RtpTcpManager::OnSocketDataImpl(RtpConnection *c, const short which) {
  if (which & EV_READ) {
    int len = buffer_read_fd_max(c->rb, c->ev_socket.fd, MAX_LEN_PER_READ);
    if (len <= 0) {
      RtpConnection::Destroy(c);
      return;
    }

    int ret = 0;
    while (true) {
      ret = OnReadPacket(c, c->rb);
      if (ret < 0) {
        RtpConnection::Destroy(c);
        return;
      }
      else if (ret == 0) {
        break;
      }
      // 收到包了，接着循环
    }
  }

  if (which & EV_WRITE) {
    if (buffer_data_len(c->wb) > 0) {
      int len = buffer_write_fd(c->wb, c->ev_socket.fd);
      if (len < 0) {
        // TODO: zhangle, destroy c in c->OnSocket function, maybe cause crash
        RtpConnection::Destroy(c);
        return;
      }
      buffer_try_adjust(c->wb);
    }

    if (buffer_data_len(c->wb) == 0) {
      c->DisableWrite();
    }
  }
}

void RtpTcpManager::StartTimer() {
  struct timeval tv;
  evtimer_set(&m_ev_timer, OnTimer, (void *)this);
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  event_base_set(m_ev_base, &m_ev_timer);
  evtimer_add(&m_ev_timer, &tv);
}

void RtpTcpManager::OnTimer(const int fd, short which, void *arg) {
  RtpTcpManager *pThis = (RtpTcpManager*)arg;
  pThis->OnTimerImpl(fd, which);
}

void RtpTcpManager::OnTimerImpl(const int fd, short which) {
  unsigned int now = time(NULL);
  std::set<RtpConnection*> trash;
  for (auto it = m_connections.begin(); it != m_connections.end();) {
    RtpConnection *c = *it;
    if (c->type != RtpConnection::CONN_TYPE_UNKOWN) {
      it = m_connections.erase(it);
      continue;
    }
    if (now - (*it)->create_time > 5) {
      trash.insert(*it);
    }
    it++;
  }
  for (auto it = trash.begin(); it != trash.end(); it++) {
    RtpConnection::Destroy(*it);
  }
  if (!m_connections.empty()) {
    StartTimer();
  }
}

//////////////////////////////////////////////////////////////////////////

RtpUdpServerManager* RtpUdpServerManager::Instance() {
  if (m_inst) {
    return m_inst;
  }
  m_inst = new RtpUdpServerManager();
  return m_inst;
}

void RtpUdpServerManager::DestroyInstance() {
  delete m_inst;
  m_inst = NULL;
}

RtpUdpServerManager::RtpUdpServerManager() {
  m_recv_buf = buffer_create_max(MAX_RTP_UDP_BUFFER_SIZE, MAX_RTP_UDP_BUFFER_SIZE);
}

RtpUdpServerManager::~RtpUdpServerManager() {
  buffer_free(m_recv_buf);
}

int RtpUdpServerManager::Init(struct event_base *ev_base) {
  m_ev_base = ev_base;

  RTPUploaderConfig *config = (RTPUploaderConfig *)ConfigManager::get_inst_config_module("rtp_uploader");
  if (NULL == config) {
    ERR("rtp uploader failed to get corresponding config information.");
    return -1;
  }

  int fd_socket = bindUdpSocket(config->listen_ip, config->listen_port);
  if (fd_socket == -1) {
    ERR("rtp uploader failed to bind udp socket.");
    return -1;
  }
  m_ev_socket.Start(ev_base, fd_socket, &RtpUdpServerManager::OnSocketData, this);

  return 0;
}

void RtpUdpServerManager::OnSocketData(const int fd, const short which, void *arg) {
  RtpUdpServerManager* pThis = (RtpUdpServerManager*)arg;
  pThis->OnSocketDataImpl(fd, which);
}

void RtpUdpServerManager::OnSocketDataImpl(const int fd, const short which) {
  if (which & EV_READ) {
    OnRead();
  }
  if (which & EV_WRITE) {
    OnWrite();
  }
}

void RtpUdpServerManager::OnRead() {
  struct sockaddr_in remote;
  struct in_addr local_ip;

  int loop = 1000;
  while (loop-- > 0) {
    buffer_reset(m_recv_buf);
    if (buffer_udp_recvmsg_fd(m_recv_buf, m_ev_socket.fd, remote, local_ip) <= 0) {
      break;
    }

    RtpConnection *c = NULL;
    auto it = m_connections.find(remote);
    if (it == m_connections.end()) {
      c = new RtpConnection();
      c->udp = true;
      c->remote = remote;
      strcpy(c->remote_ip, inet_ntoa(c->remote.sin_addr));
      c->local_ip = local_ip;
      c->manager = this;
      m_connections[c->remote] = c;
    }
    else {
      c = it->second;
    }

    if (OnReadPacket(c, m_recv_buf) < 0) {
      RtpConnection::Destroy(c);
    }
  }
}

void RtpUdpServerManager::OnWrite() {
  while (!m_send_queue.is_queue_empty()) {
    m_send_queue.send(m_ev_socket.fd);
  }
  DisableWrite();
}

void RtpUdpServerManager::DisableWrite() {
  m_ev_socket.DisableWrite();
}

void RtpUdpServerManager::EnableWrite() {
  m_ev_socket.EnableWrite();
}

int RtpUdpServerManager::SendUdpData(RtpConnection *c, const unsigned char *data, int len) {
  return m_send_queue.add(c->remote.sin_addr.s_addr, c->remote.sin_port, (const char *)data, len);
}

void RtpUdpServerManager::OnConnectionClosed(RtpConnection *c) {
  m_connections.erase(c->remote);
  m_trans_mgr->_close_trans(c);
}

RtpUdpServerManager* RtpUdpServerManager::m_inst = NULL;

//////////////////////////////////////////////////////////////////////////

void LibEventSocket::Start(struct event_base* ev_base, int fd, LibEventCb cb, void *cb_arg) {
  this->ev_base = ev_base;
  this->fd = fd;
  this->cb = cb;
  this->cb_arg = cb_arg;
  util_set_nonblock(fd, TRUE);
  event_set(&ev_socket, fd, EV_READ | EV_WRITE | EV_PERSIST, cb, cb_arg);
  event_base_set(ev_base, &ev_socket);
  event_add(&ev_socket, NULL);
  w_enabled = true;
}

void LibEventSocket::Stop() {
  event_del(&ev_socket);
  close(fd);
  fd = -1;
}

void LibEventSocket::EnableWrite() {
  if (!w_enabled) {
    event_del(&ev_socket);
    event_set(&ev_socket, fd, EV_READ | EV_WRITE | EV_PERSIST, cb, cb_arg);
    event_base_set(ev_base, &ev_socket);
    event_add(&ev_socket, NULL);
    w_enabled = true;
  }
}

void LibEventSocket::DisableWrite() {
  event_del(&ev_socket);
  event_set(&ev_socket, fd, EV_READ | EV_PERSIST, cb, cb_arg);
  event_base_set(ev_base, &ev_socket);
  event_add(&ev_socket, NULL);
  w_enabled = false;
}

//////////////////////////////////////////////////////////////////////////

RtpTcpServerManager* RtpTcpServerManager::Instance() {
  if (m_inst) {
    return m_inst;
  }
  m_inst = new RtpTcpServerManager();
  return m_inst;
}

void RtpTcpServerManager::DestroyInstance() {
  delete m_inst;
  m_inst = NULL;
}

int32_t RtpTcpServerManager::Init(struct event_base *ev_base) {
  uploader_config *config = (uploader_config *)ConfigManager::get_inst_config_module("uploader");
  if (NULL == config) {
    ERR("rtp uploader failed to get corresponding config information.");
    return -1;
  }

  m_ev_base = ev_base;

  int fd_socket = util_create_listen_fd("0.0.0.0", config->listen_port, 128);
  if (fd_socket < 0)	{
    ERR("create rtp tcp listen fd failed. ret = %d", fd_socket);
    return -1;
  }
  m_ev_socket.Start(ev_base, fd_socket, &RtpTcpServerManager::OnSocketAccept, this);

  //TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");
  //if (common_config->enable_rtp2flv) {
  //  m_rtp_cache = media_manager::RTP2FLVRemuxer::get_instance();
  //}
  //else {
  //  m_rtp_cache = new RTPMediaManagerHelper;
  //}
  m_trans_mgr = new RTPTransManager(RtpCacheManager::Instance());

  start_timer();

  return 0;
}

void RtpTcpServerManager::OnSocketAccept(const int fd, const short which, void *arg) {
  RtpTcpServerManager *pThis = (RtpTcpServerManager*)arg;
  pThis->OnSocketAcceptImpl(fd, which);
}

void RtpTcpServerManager::OnSocketAcceptImpl(const int fd, const short which) {
  if (which & EV_READ) {
    struct sockaddr_in remote;
    socklen_t len = sizeof(struct sockaddr_in);
    memset(&remote, 0, len);
    int fd_socket = ::accept(fd, (struct sockaddr *)&remote, &len);
    if (fd_socket < 0) {
      WRN("uploader bin accept failed. fd_socket = %d, error = %s",
        fd_socket, strerror(errno));
      return;
    }

    CreateConnection(&remote, fd_socket);
  }
}

int RtpTcpServerManager::set_http_server(http::HTTPServer *http_server) {
  http_server->add_handle("/upload/sdp",
    http_put_sdp,
    this,
    http_put_sdp_check,
    this,
    NULL,
    NULL);
  http_server->add_handle("/download/sdp",
    http_get_sdp_handle,
    this,
    http_get_sdp_check_handle,
    this,
    NULL,
    NULL);
  http_server->add_handle("/rtp/count",
    get_count_handler,
    this,
    get_count_check_handler,
    this,
    NULL,
    NULL);
  return 0;
}

void RtpTcpServerManager::http_put_sdp(http::HTTPConnection* conn, void* args) {
  http::HTTPRequest* request = conn->request;
  if (conn->get_to_read() != 0) {
    return;
  }

  string path = request->uri;
  char buf[128];
  if (!util_path_str_get(path.c_str(), path.length(), 3, buf, sizeof(buf))) {
    ERR("wrong path, cannot get stream id.");
    util_http_rsp_error_code(conn->get_fd(), HTTP_404);
    conn->stop();
    return;
  }
  buf[32] = '\0';
  StreamId_Ext stream_id;
  if (!util_check_hex(buf, 32) || (stream_id.parse(buf, 16) != 0)) {
    ERR("wrong path, cannot get stream id.");
    util_http_rsp_error_code(conn->get_fd(), HTTP_404);
    conn->stop();
    return;
  }

  string sdp_str;
  sdp_str.assign(buffer_data_ptr(conn->_read_buffer), buffer_data_len(conn->_read_buffer));

  RtpTcpServerManager* pThis = (RtpTcpServerManager*)args;
  int status_code = -1;
  pThis->m_trans_mgr->set_sdp_str(stream_id, sdp_str, status_code);

  INF("http put sdp complete, stream_id: %s, sdp_len: %ld sdp:%s", stream_id.unparse().c_str(), sdp_str.length(), sdp_str.c_str());
  util_http_rsp_error_code(conn->get_fd(), HTTP_200);
  conn->stop();
  return;
}

void RtpTcpServerManager::http_put_sdp_check(http::HTTPConnection*, void*) {
}

void RtpTcpServerManager::get_count_handler(http::HTTPConnection* conn, void* args) {
}

void RtpTcpServerManager::get_count_check_handler(http::HTTPConnection* conn, void* args) {
}

void RtpTcpServerManager::http_get_sdp_handle(http::HTTPConnection* conn, void* args) {
  http::HTTPRequest* request = conn->request;

  string path = request->uri;
  char buf[128];
  if (!util_path_str_get(path.c_str(), path.length(), 3, buf, sizeof(buf))) {
    ERR("wrong path, cannot get stream id.");
    util_http_rsp_error_code(conn->get_fd(), HTTP_404);
    conn->stop();
    return;
  }
  buf[32] = '\0';
  StreamId_Ext stream_id;
  if (!util_check_hex(buf, 32) || (stream_id.parse(buf, 16) != 0)) {
    ERR("wrong path, cannot get stream id.");
    util_http_rsp_error_code(conn->get_fd(), HTTP_404);
    conn->stop();
    return;
  }

  int status_code = -1;
  RtpTcpServerManager *pThis = (RtpTcpServerManager*)args;
  string sdp_str = pThis->m_trans_mgr->get_sdp_str(stream_id, status_code);
  if (sdp_str.length() > 0) {
    char rsp[1024];
    snprintf(rsp, sizeof(rsp),
      "HTTP/1.0 200 OK\r\nServer: Youku Live Forward\r\n"
      "Content-Type: application/sdp\r\n"
      "Host: %s:%d\r\n"
      "Connection: Close\r\n"
      "Expires: -1\r\nCache-Control: private, max-age=0\r\nPragma: no-cache\r\n"
      "Content-Length: %zu\r\n\r\n",
      conn->get_remote_ip(), conn->get_remote_port(), sdp_str.length());

    if (conn->writeResponseString(rsp) < 0) {
      WRN("player-sdp %s:%6hu %d %s 404",
        conn->get_remote_ip(), conn->get_remote_port(), request->method, request->uri.c_str());
      return;
    }

    conn->writeResponseString(sdp_str);
    DBG("player-sdp %s:%6hu %d %s 200",
      conn->get_remote_ip(), conn->get_remote_port(), request->method, request->uri.c_str());
    conn->stop();
  }
  else {
    ERR("failed to get sdp, empty sdpstr streamid = %u", stream_id.get_32bit_stream_id());
    util_http_rsp_error_code(conn->get_fd(), HTTP_404);
    conn->stop();
  }
}

void RtpTcpServerManager::http_get_sdp_check_handle(http::HTTPConnection* conn, void* args) {
}

void RtpTcpServerManager::start_timer() {
  struct timeval tv;
  evtimer_set(&m_ev_timer, timer_cb, (void *)this);
  tv.tv_sec = 0;
  tv.tv_usec = 10000;
  event_base_set(m_ev_base, &m_ev_timer);
  evtimer_add(&m_ev_timer, &tv);
}

void RtpTcpServerManager::timer_cb(const int fd, short which, void *arg) {
  RtpTcpServerManager *p = (RtpTcpServerManager*)arg;
  p->m_trans_mgr->on_timer();
  p->start_timer();
}

RtpTcpServerManager* RtpTcpServerManager::m_inst = NULL;
