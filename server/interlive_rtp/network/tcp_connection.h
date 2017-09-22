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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <errno.h>

#include "utils/buffer.hpp"

namespace network
{
    class TCPServer;
    class TCPConnection
    {
        friend class TCPServer;
    public:
        TCPConnection(TCPServer*, event_base*);
        virtual ~TCPConnection();
        int get_fd();
        virtual int init(int fd, sockaddr_in& s_in, int read_buffer_max, int write_buffer_max);
        virtual const char *get_remote_ip() const;
        virtual uint16_t get_remote_port();

        virtual void close();
        virtual void enable_write();
        virtual bool is_alive();

    protected:
        virtual void _on_read(const int fd, const short which, void* arg);
        virtual void _on_write(const int fd, const short which, void* arg);
        virtual void _on_timeout(const int fd, const short which, void* arg);

        static void _static_on_read(const int fd, const short which, void* arg);
        static void _static_on_write(const int fd, const short which, void* arg);

    public:
        buffer *_read_buffer;
        buffer *_write_buffer;

    protected:
        int _fd;

        event _read_ev;
        event _write_ev;

        char _local_ip[16];		/* address to use for binding the src */
        uint16_t _local_port;		/* local port for binding the src */

        char _remote_ip[16];			/* address to connect to */
        uint16_t _remote_port;

        int _timeout;			/* timeout in seconds for events */

        int64_t _to_read;

        TCPServer* _tcp_server;

        uint64_t _active_tick_ms;

        event_base *_main_base;
    };
}
