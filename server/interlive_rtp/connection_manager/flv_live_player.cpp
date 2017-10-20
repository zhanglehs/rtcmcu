#include "flv_live_player.h"

#include "../util/util.h"
#include "../util/log.h"
#include "../util/access.h"
#include "fragment/fragment.h"
#include "../util/connection.h"
#include "target.h"
#include "media_manager/cache_manager.h"
#include "connection_manager/FlvConnectionManager.h"

#define HTTP_CONNECTION_HEADER "Connection: Close\r\n"
#define NOCACHE_HEADER "Expires: -1\r\nCache-Control: private, max-age=0\r\nPragma: no-cache\r\n"

//////////////////////////////////////////////////////////////////////////

BaseLivePlayer::BaseLivePlayer(LiveConnection *c) {
  m_connection = c;
}

BaseLivePlayer::~BaseLivePlayer() {
}

//////////////////////////////////////////////////////////////////////////

FlvLivePlayer::FlvLivePlayer(LiveConnection *c) : BaseLivePlayer(c) {
  m_written_header = false;
  m_written_first_tag = false;
  m_buffer_overuse = false;
  m_latest_blockid = -1;
  _cmng = media_manager::FlvCacheManager::Instance();
}

void FlvLivePlayer::OnWrite() {
  LiveConnection *c = m_connection;
  StreamId_Ext streamid = c->streamid;

  if (!m_written_header) {
    fragment::FLVHeader flvheader(streamid);
    if (_cmng->get_miniblock_flv_header(streamid, flvheader) < 0) {
      WRN("req live header failed, streamid= %s", streamid.unparse().c_str());
      c->DisableWrite();
      return;
    }

    // reponse header
    char rsp[1024];
    int len = snprintf(rsp, sizeof(rsp),
      "HTTP/1.1 200 OK\r\n"
      "Server: Youku Live Forward\r\n"
      "Content-Type: video/x-flv\r\n"
      NOCACHE_HEADER
      HTTP_CONNECTION_HEADER "\r\n");
    if (buffer_append_ptr(c->wb, rsp, len) < 0) {
      LiveConnection::Destroy(c);
      return;
    }

    flvheader.copy_header_to_buffer(c->wb);

    m_written_header = true;
  }

  const int kBufferOveruseSize = 2 * 1024 * 1024;
  if (buffer_data_len(c->wb) > kBufferOveruseSize) {
    // buffer超限，下次从IDR帧开始发送
    m_buffer_overuse = true;
    m_written_first_tag = false;
  }
  else if (m_buffer_overuse && buffer_data_len(c->wb) < kBufferOveruseSize / 2) {
    // buffer从超限到正常的判断
    m_buffer_overuse = false;
  }
  if (m_buffer_overuse) {
    // buffer超限时丢弃frame
    return;
  }

  if (!m_written_first_tag) {
    fragment::FLVMiniBlock* block = _cmng->get_latest_miniblock(streamid);
    if (block) {
      m_latest_blockid = block->get_seq();
      uint32_t timeoffset = 0;
      block->copy_payload_to_buffer(c->wb, timeoffset, FLV_FLAG_BOTH);
      m_written_first_tag = true;
    }
    else {
      return;
    }
  }

  fragment::FLVMiniBlock* block = NULL;
  while ((block = _cmng->get_miniblock_by_seq(streamid, m_latest_blockid + 1)) != NULL) {
    m_latest_blockid = block->get_seq();
    uint32_t timeoffset = 0;
    block->copy_payload_to_buffer(c->wb, timeoffset, FLV_FLAG_BOTH);
  }
}

//////////////////////////////////////////////////////////////////////////

CrossdomainLivePlayer::CrossdomainLivePlayer(LiveConnection *c, const FlvPlayerConfig *config) : BaseLivePlayer(c), m_config(config) {
}

void CrossdomainLivePlayer::OnWrite() {
  LiveConnection *c = m_connection;

  char rsp[1024];
  int used = snprintf(rsp, sizeof(rsp),
    "HTTP/1.0 200 OK\r\n"
    "Server: Youku Live Forward\r\n"
    "Content-Type: application/xml;charset=utf-8\r\n"
    NOCACHE_HEADER
    HTTP_CONNECTION_HEADER
    "Content-Length: %zu\r\n\r\n",
    m_config->crossdomain_len);

  if (0 != buffer_expand_capacity(c->wb, m_config->crossdomain_len + used)) {
    ACCESS("crossdomain %s:%d GET /crossdomain.xml 500",
      c->remote_ip, (int)c->remote.sin_port);
    util_http_rsp_error_code(c->ev_socket.fd, HTTP_500);
    //conn_close(c);
    return;
  }
  buffer_append_ptr(c->wb, rsp, used);
  buffer_append_ptr(c->wb, m_config->crossdomain, m_config->crossdomain_len);

  ACCESS("crossdomain %s:%d GET /crossdomain.xml 200",
    c->remote_ip, (int)c->remote.sin_port);
}
