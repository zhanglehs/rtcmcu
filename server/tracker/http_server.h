/**
 * file: http_server.h
 * brief:   a simple http server using libevent.
 * author: duanbingnan
 * copyright: Youku
 * email: duanbingnan@youku.com
 * date: 2015/03/02
 */
#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

#include <sys/queue.h>
#include <event.h>
#include <evhttp.h>
#include <string.h>
#include <util/util.h>

#include "forward_manager_v3.h"

void root_handler(struct evhttp_request *req, void *arg);
void query_handler(struct evhttp_request *req, void *arg);
void stat_handler(struct evhttp_request *req, void *arg);
void stat_str_handler(struct evhttp_request *req, void *arg);
void generic_handler(struct evhttp_request *req, void *arg);
int tracker_start_http_server(struct event_base *base);

#endif  /* HTTP_SERVER_H_ */

