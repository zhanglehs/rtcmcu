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

#include "base_tcp_server.h"
#include <string>
#include <assert.h>
#include <stdlib.h>

#include "util/log.h"
#include "util/access.h"
#include "util/util.h"

using namespace std;

namespace network
{
    TCPServer::~TCPServer()
    {

    }

    int TCPServer::init(event_base* main_base)
    {
        if (NULL == main_base)
        {
            ERR("main_base is NULL. main_base = %p", main_base);
            return -1;
        }
        _main_base = main_base;

        _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (-1 == _listen_fd) {
            ERR("socket failed. error = %s", strerror(errno));
            return -1;
        }
        int val = 1;
        if (-1 == setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR,
            (void*)&val, sizeof(val))) {
            ERR("set socket SO_REUSEADDR failed. error = %s", strerror(errno));
            return -1;
        }
        if (0 != _set_sockopt(_listen_fd)) {
            ERR("set _listen_fd sockopt failed.");
            return -1;
        }
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        inet_aton("0.0.0.0", &(addr.sin_addr));
        addr.sin_port = htons(_listen_port);

        int ret = ::bind(_listen_fd, (struct sockaddr *) &addr, sizeof(addr));
        if (0 != ret) {
            ERR("bind addr failed. error = %s", strerror(errno));
            return -8;
        }

        ret = listen(_listen_fd, 1024);
        if (0 != ret) {
            ERR("listen http fd failed. error = %s", strerror(errno));
            return -9;
        }

        event_set(&_ev_listener, _listen_fd, EV_READ | EV_PERSIST, _static_accept, this);
        event_base_set(_main_base, &_ev_listener);
        event_add(&_ev_listener, 0);

        return 0;
    }

    void TCPServer::_static_accept(const int fd, const short which, void* arg)
    {
        TCPServer* server = (TCPServer*)arg;
        server->_accept(fd, which, arg);
    }

    const char* TCPServer::_get_server_name()
    {
        return "tcp";
    }

    void TCPServer::_accept_error(const int fd, struct sockaddr_in& s_in)
    {

    }

    TCPConnection* TCPServer::_create_conn()
    {
        return new TCPConnection(this, this->_main_base);
    }

    void TCPServer::_accept(const int fd, const short which, void* arg)
    {
        TRC("%s accept,fd = %d", _get_server_name(), fd);
        TCPConnection *conn = NULL;
        int newfd = -1;
        struct sockaddr_in s_in;
        socklen_t len = sizeof(struct sockaddr_in);

        memset(&s_in, 0, len);
        newfd = ::accept(fd, (struct sockaddr *) &s_in, &len);
        if (-1 == newfd)
        {
            ERR("fd #%d accept() failed, error = %s", fd, strerror(errno));
            return;
        }

        // TODO: if (newfd >= _config->max_conns)

        if (0 != _set_sockopt(newfd))
        {
            ERR("set newfd sockopt failed. fd = %d", newfd);
            _accept_error(newfd, s_in);
            close(newfd);
            return;
        }

        conn = _create_conn();
        if (conn == NULL)
        {
            ERR("malloc %s connection error", _get_server_name());
            _accept_error(newfd, s_in);
            close(newfd);
            return;
        }

        if (0 != conn->init(newfd, s_in, 1024 * 1024, 1024 * 1024))
        {
            ERR("%s conn init failed. fd = %d", _get_server_name(), newfd);
            _accept_error(newfd, s_in);
            _close_conn(conn);
            return;
        }

        _conn_set.insert(conn);

        event_set(&(conn->_read_ev), conn->_fd, EV_READ | EV_PERSIST, TCPConnection::_static_on_read, (void *)conn);
        event_base_set(_main_base, &(conn->_read_ev));
        event_add(&(conn->_read_ev), 0);

        event_set(&(conn->_write_ev), conn->_fd, EV_WRITE, TCPConnection::_static_on_read, (void *)conn);
        event_base_set(_main_base, &(conn->_write_ev));

        INF("%s connected #%d(%s:%d)", _get_server_name(), conn->_fd, conn->_remote_ip, conn->_remote_port);
    }

    int TCPServer::_set_sockopt(int fd)
    {
        util_set_cloexec(fd, TRUE);
        int ret = util_set_nonblock(fd, TRUE);
        if (0 != ret) {
            ERR("set nonblock failed. ret = %d, fd = %d, error = %s", ret, fd,
                strerror(errno));
            return -1;
        }
        int val = 1;
        if (-1 == setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&val,
            sizeof(val))) {
            ERR("set tcp nodelay failed. fd = %d, error = %s", fd, strerror(errno));
            return -1;
        }
#ifdef TCP_DEFER_ACCEPT
        {
            int v = 30; /* 30 seconds */

            if (-1 == setsockopt(fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &v,
                sizeof(v))) {
                WRN("can't set TCP_DEFER_ACCEPT on http_listen_fd %d",
                    fd);
            }
        }
#endif
        return 0;
    }

    int TCPServer::_close_conn(TCPConnection * conn)
    {
        conn->close();
        _conn_set.erase(conn);
        delete conn;
        return 0;
    }
}
