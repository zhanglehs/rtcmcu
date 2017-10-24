#include "rtp_send_queue.h" 

#include "../util/log.h"
#include "common_defs.h"
#include "../util/buffer.hpp"

typedef struct RTPSendQueueSlot {
  uint32_t net_ip;
  uint16_t net_port;
  unsigned char data[MAX_RTP_UDP_BUFFER_SIZE];
  int len;
} RTPSendQueueSlot;

RTPSendQueue::~RTPSendQueue() {
  for (auto it = _send_queue.begin(); it != _send_queue.end(); it++) {
    delete *it;
  }
  _send_queue.clear();
  for (auto it = _free_slot_list.begin(); it != _free_slot_list.end(); it++) {
    delete *it;
  }
  _free_slot_list.clear();
}

int RTPSendQueue::send(int fd) {
  while (!_send_queue.empty()) {
    RTPSendQueueSlot * slot = _send_queue.front();
    struct sockaddr_in sock_addr;
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = slot->net_ip;
    sock_addr.sin_port = slot->net_port;
    sendto(fd, slot->data, slot->len, 0, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    _send_queue.pop_front();
    RecycleSlot(slot);
  }
  
  return 0;
}

int RTPSendQueue::add(uint32_t net_ip, uint16_t net_port, const char *buf, int len) {
  if (len > MAX_RTP_UDP_BUFFER_SIZE || len <= 0 || buf == NULL) {
    return -1;
  }
  RTPSendQueueSlot *slot = CreateSlot();
  if (slot) {
    slot->net_ip = net_ip;
    slot->net_port = net_port;
    memcpy(slot->data, buf, len);
    slot->len = len;
    _send_queue.push_back(slot);
    return 0;
  }

  return -1;
}

RTPSendQueueSlot * RTPSendQueue::CreateSlot() {
  if (0 != _free_slot_list.size()) {
    RTPSendQueueSlot * slot = _free_slot_list.front();
    _free_slot_list.pop_front();
    return slot;
  }
  RTPSendQueueSlot * slot = new RTPSendQueueSlot;
  return slot;
}

void RTPSendQueue::RecycleSlot(RTPSendQueueSlot *slot) {
  if (slot) {
    if (_free_slot_list.size() > 10000) {
      delete slot;
    }
    else {
      _free_slot_list.push_back(slot);
    }
  }
}
