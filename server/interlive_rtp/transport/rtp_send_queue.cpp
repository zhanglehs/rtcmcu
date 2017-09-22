#include "util/util.h"
#include "util/log.h"
#include "util/access.h"
#include "util/levent.h"
#include "utils/memory.h"
#include "common_defs.h"
#include "rtp_send_queue.h" 
#include "rtp_trans/rtp_media_manager_helper.h"
#include "target.h"

#include <assert.h>
#include <errno.h>

using namespace std;

bool RTPSendQueue::is_queue_empty()
{
    return _send_queue.empty();
}

RTPSendQueueSlot * RTPSendQueue::add()
{
    RTPSendQueueSlot *slot = _create_slot();
    if (slot)
    {
        _send_queue.push_back(slot);
    }
    return slot;
}

int RTPSendQueue::send(int fd)
{
    RTPSendQueueSlot * slot = _send_queue.front();
    if (NULL != slot) {
        struct sockaddr_in sock_addr;
        sock_addr.sin_family = AF_INET;
        sock_addr.sin_addr.s_addr = slot->net_ip;
        sock_addr.sin_port = slot->net_port;
        const char * buf = buffer_data_ptr(slot->wb);
        int len = buffer_data_len(slot->wb);
        int ret = sendto(fd, buf, len, 0, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
        if (ret > 0 || (len == 0 && ret == 0))
        {
			if (len == 0 )
			{
				WRN("slot len 0 ip %d port %d",slot->net_ip,slot->net_port);
			}
            _send_queue.pop_front();
            _destroy_slot(slot);
        }
    }
    return 0;
}

int RTPSendQueue::add(uint32_t net_ip, uint16_t net_port, const char *buf, int len)
{
    RTPSendQueueSlot *slot = _create_slot();
    if (slot)
    {
        slot->net_ip = net_ip;
        slot->net_port = net_port;
        buffer_append_ptr(slot->wb, buf, len);
        _send_queue.push_back(slot);
        return 0;
    }

    return -1;
}

void RTPSendQueue::_release_send_queue()
{
    deque<RTPSendQueueSlot *>::iterator it;
    for (it = _send_queue.begin(); it != _send_queue.end(); it++) {
        if (NULL != *it && NULL != (*it)->wb){
            buffer_free((*it)->wb);
        }
    }
    _send_queue.clear();
}

RTPSendQueueSlot * RTPSendQueue::_create_slot()
{
    if (0 != _free_slot_list.size())
    {
        RTPSendQueueSlot * slot = _free_slot_list.front();
        _free_slot_list.pop_front();
        return slot;
    }
    RTPSendQueueSlot * slot = new RTPSendQueueSlot;
    if (NULL != slot)
        slot->wb = buffer_create_max(MAX_RTP_UDP_BUFFER_SIZE, MAX_RTP_UDP_BUFFER_SIZE);
    return slot;
}

void RTPSendQueue::_destroy_slot(RTPSendQueueSlot *slot)
{
    if (slot)
    {
        buffer_reset(slot->wb);
        _free_slot_list.push_back(slot);
    }
}


