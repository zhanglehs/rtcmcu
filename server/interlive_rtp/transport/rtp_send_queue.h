/***********************************************************************
* author: zhangcan
* email: zhangcan@youku.com
* date: 2015-08-11
* copyright:youku.com
************************************************************************/

#ifndef _RTP_SERVER_BASE_H_
#define _RTP_SERVER_BASE_H_

#include <string>
#include <deque>
#include <list>
//#include <ext/hash_map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include "utils/buffer.hpp"
#include "util/levent.h"
#include "rtp_trans/rtp_trans_manager.h"

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
private:
    std::deque<RTPSendQueueSlot *> _send_queue;
    std::deque<RTPSendQueueSlot *> _free_slot_list;
};

#endif /* _RTP_SERVER_BASE_H_ */
