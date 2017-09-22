/**
* @file
* @brief
* @author   songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2015/07/24
* @see
*/

#pragma once

#include <event.h>
#include "tcp_connection.h"
#include <set>

namespace network
{
    class TCPServer
    {
        friend class TCPConnection;
    public:
        virtual int init(event_base* main_base);
        virtual ~TCPServer();

    protected:
        static void _static_accept(const int fd, const short which, void* arg);
        virtual void _accept(const int fd, const short which, void* arg);

        virtual int _set_sockopt(int fd);
        virtual int _close_conn(TCPConnection * c);

        virtual const char* _get_server_name();
        virtual void _accept_error(const int fd, struct sockaddr_in& s_in);
        virtual TCPConnection* _create_conn();

    protected:
        event_base* _main_base;

        int _listen_fd;
        uint16_t _listen_port;
        struct event _ev_listener;

        typedef std::set<TCPConnection*> ConnectionSet_t;
        ConnectionSet_t _conn_set;
    };
}
