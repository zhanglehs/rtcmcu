#include "connection_manager/RtpConnection.h"

#include "connection_manager/RtpConnectionManager.h"

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
