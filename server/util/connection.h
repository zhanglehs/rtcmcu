/****************************************
 * YueHonghui, 2013-07-27
 * hhyue@tudou.com
 * copyright:youku.com
 * *************************************/
#ifndef CONNECTION_H_
#define CONNECTION_H_
#include <unistd.h>
#include <stdint.h>
#include <event.h>
#include "utils/buffer.hpp"
#include "session.h"

#if (defined __cplusplus && !defined _WIN32)
extern "C" 
{
#endif
typedef struct connection
{
    int fd;
    struct event ev;
    buffer *r;
    buffer *w;
    char remote_ip[16];
    uint16_t remote_port;
    char local_ip[16];
    uint16_t local_port;
    void *bind;
    int bind_flag;
    session timer;
} connection;

int
conn_init(connection * c, int fd, size_t buffer_max,
          struct event_base *eb, void (*handler) (int fd, short which,
                                                  void *arg),
          session_manager * sm, int timeout_sec, int bind_flag);

void conn_close(connection * c);

void
conn_enable_write(connection * c, struct event_base *eb,
                  void (*handler) (int fd, short which, void *arg));

void
conn_disable_write(connection * c, struct event_base *eb,
                   void (*handler) (int fd, short which, void *arg));

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
