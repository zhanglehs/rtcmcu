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

#include "tcp_connection.h"
#include "base_tcp_server.h"
#include "util/util.h"
#include "util/log.h"

namespace network
{
    TCPConnection::TCPConnection(TCPServer* server, event_base* base)
    {
        _fd = -1;
        _read_buffer = NULL;
        _write_buffer = NULL;

        _local_port = 0;

        _remote_port = 0;

        _to_read = -1;

        _tcp_server = server;

        _main_base = base;
    }

    TCPConnection::~TCPConnection()
    {
        if (_read_buffer != NULL)
        {
            buffer_free(_read_buffer);
            _read_buffer = NULL;
        }

        if (_write_buffer != NULL)
        {
            buffer_free(_write_buffer);
            _write_buffer = NULL;
        }
    }

    int TCPConnection::get_fd()
    {
        return _fd;
    }

    int TCPConnection::init(int socket, sockaddr_in& s_in, int read_buffer_max, int write_buffer_max)
    {
        _fd = socket;
        memset(_remote_ip, 0, sizeof(_remote_ip));
        util_ip2str_no_r(s_in.sin_addr.s_addr, _remote_ip, sizeof(_remote_ip));
        _remote_port = ntohs(s_in.sin_port);

        _read_buffer = buffer_create_max(4096, read_buffer_max);
        if (NULL == _read_buffer)
        {
            ERR("buffer_create_max failed.");
            _fd = -1;
            return -1;
        }
        _write_buffer = buffer_create_max(4096, write_buffer_max);
        if (NULL == _write_buffer)
        {
            ERR("buffer_create_max failed.");
            buffer_free(_read_buffer);
            _read_buffer = NULL;
            _fd = -1;
            return -1;
        }

        return 0;
    }

    const char *TCPConnection::get_remote_ip() const
    {
        return _remote_ip;
    }

    uint16_t TCPConnection::get_remote_port()
    {
        return _remote_port;
    }

    void TCPConnection::close()
    {
        if (event_initialized(&_write_ev))
        {
            event_del(&_write_ev);
        }

        if (event_initialized(&_read_ev))
        {
            event_del(&_read_ev);
        }

        if (_fd != -1)
        {
            ::close(_fd);
            _fd = -1;
        }

        if (_read_buffer != NULL)
        {
            buffer_free(_read_buffer);
            _read_buffer = NULL;
        }

        if (_write_buffer != NULL)
        {
            buffer_free(_write_buffer);
            _write_buffer = NULL;
        }
    }

    void TCPConnection::enable_write()
    {
        if (event_pending(&_write_ev, EV_WRITE, NULL))
        {
            return;
        }

        event_add(&_write_ev, 0);
    }

    bool TCPConnection::is_alive()
    {
        return true;
    }

    void TCPConnection::_static_on_read(const int fd, const short which, void* arg)
    {
        TCPConnection *conn = (TCPConnection *)arg;
        TCPServer* server = conn->_tcp_server;
        conn->_on_read(fd, which, arg);

        if (!conn->is_alive())
        {
            server->_close_conn(conn);
        }
    }

    void TCPConnection::_static_on_write(const int fd, const short which, void* arg)
    {
        TCPConnection *conn = (TCPConnection *)arg;
        TCPServer* server = conn->_tcp_server;
        conn->_on_write(fd, which, arg);

        if (!conn->is_alive())
        {
            server->_close_conn(conn);
        }
    }

    void TCPConnection::_on_read(const int fd, const short which, void* arg)
    {

    }

    void TCPConnection::_on_write(const int fd, const short which, void* arg)
    {

    }

    void TCPConnection::_on_timeout(const int fd, const short which, void* arg)
    {

    }

}
