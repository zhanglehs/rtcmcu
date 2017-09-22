/*
 *
 * author: hechao@youku.com
 * create: 2014/3/17
 *
 */

#ifndef __HTTP_CONNECTION_H_
#define __HTTP_CONNECTION_H_

#include "levent.h"
#include <string>
#include <map>
#include <utils/buffer.hpp>
#include "gettable.h"
#include "postable.h"

enum CTX_STATE
{
    CTX_STATE_INIT = 0,
    CTX_STATE_READING,
    CTX_STATE_SENDING,
    CTX_STATE_END,
};

class AdminServer;

class HTTPConnection
{
    public:
        HTTPConnection(int fd,
                Getable *getter, Postable *poster, AdminServer * admin,
                size_t buf_size = 100*1024);
        ~HTTPConnection();

        int init();
        int start();

        void http_handler(int fd, const short event);
        void http_read_handler(int fd, const short event);
        void http_write_handler(int fd, const short event);
        void set_event_base(struct event_base *base)
        {
            _event_base = base;
        }

        static void timer_event_handler(const int fd, const short event, void *ctx);
        void on_timer(const int fd, const short event);

    private:
        void _finish();
        int _parse_http_req(lcdn::base::Buffer * buf);
        void _enable_write();
        void _disable_write();
        void _stop();
        int  _idle_time();
        void _start_timer();
        void _start_timer(uint32_t sec, uint32_t usec);
        void _stop_timer();

    public:
        bool stopped;

    private:
        int _fd;
        size_t _buf_size;
        lcdn::base::Buffer *_read_buf;
        lcdn::base::Buffer *_write_buf;

        CTX_STATE _state;

        struct event _event;
        bool _is_ev_monitored;
        struct event _timer_event;
        bool _timer_enable;
        struct event_base * _event_base;

        Getable *_getter;
        Postable *_poster;
        AdminServer *_admin;

        int _last_active_time;

};

#endif

