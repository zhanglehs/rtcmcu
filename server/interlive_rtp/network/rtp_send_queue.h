#ifndef _RTP_SEND_QUEUE_H_
#define _RTP_SEND_QUEUE_H_

#include <stdint.h>
#include <deque>

struct RTPSendQueueSlot;

class RTPSendQueue {
public:
  RTPSendQueue() {}
  ~RTPSendQueue();
  int send(int fd);
  int add(uint32_t net_ip, uint16_t net_port, const char *buf, int len);

private:
  RTPSendQueueSlot * CreateSlot();
  void RecycleSlot(RTPSendQueueSlot *slot);

  std::deque<RTPSendQueueSlot *> _send_queue;
  std::deque<RTPSendQueueSlot *> _free_slot_list;
};

#endif
