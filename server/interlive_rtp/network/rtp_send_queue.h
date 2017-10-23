/***********************************************************************
* author: zhangcan
* email: zhangcan@youku.com
* date: 2015-08-11
* copyright:youku.com
************************************************************************/

#ifndef _RTP_SERVER_BASE_H_
#define _RTP_SERVER_BASE_H_

#include "../util/buffer.hpp"
#include <deque>

typedef struct RTPSendQueueSlot {
  uint32_t ip;
  uint32_t net_ip;
  uint16_t port;
  uint16_t net_port;
  buffer * wb;
} RTPSendQueueSlot;

class RTPSendQueue {
public:
  RTPSendQueue() {}
  ~RTPSendQueue() {
    _release_send_queue();
  }
  bool is_queue_empty();
  RTPSendQueueSlot * add();
  int send(int fd);
  int add(uint32_t net_ip, uint16_t net_port, const char *buf, int len);

private:
  RTPSendQueueSlot * _create_slot();
  void _destroy_slot(RTPSendQueueSlot *slot);
  void _release_send_queue();

  std::deque<RTPSendQueueSlot *> _send_queue;
  std::deque<RTPSendQueueSlot *> _free_slot_list;
};

#endif /* _RTP_SERVER_BASE_H_ */
