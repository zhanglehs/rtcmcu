/*
 * author: hechao@youku.com
 * create: 2013.7.18
 */

#ifndef TRACKER_STAT_H
#define TRACKER_STAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" 
{
#endif

typedef struct tracker_stat
{
    uint32_t cur_stream_num;
    uint32_t cur_forward_num;
    uint32_t register_cnt;
    uint32_t keep_req_cnt;
    uint32_t add_stream_cnt;
    uint32_t del_stream_cnt;
    uint32_t req_addr_cnt;
}tracker_stat_t;

extern tracker_stat_t * g_tracker_stat;

int tracker_stat_init(const char *log_svr_ip, uint16_t log_svr_port);
void tracker_stat_stop();
void tracker_stat_dump_and_reset(tracker_stat_t * stat);

#ifdef __cplusplus
}
#endif
#endif

