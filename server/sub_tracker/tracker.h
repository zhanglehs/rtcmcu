/*
 * author: hechao@youku.com
 * create: 2013.5.21
 *
 */

#ifndef FORWARD_TRACKER_H
#define FORWARD_TRACKER_H

#include <stdint.h>
#include <event.h>

#include "event_loop.h"
#include "tracker_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

int tracker_init(tracker_config_t * config);
int tracker_start_tracker(lcdn::net::EventLoop* loop, struct event_base *base);
void tracker_stop_tracker();
void tracker_dump_state();

#ifdef __cplusplus
}
#endif
#endif

