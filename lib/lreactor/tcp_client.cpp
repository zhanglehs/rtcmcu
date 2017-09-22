#include "tcp_client.h"
#include "sockets_ops.h"
#include "tcp_connection.h"
#include "utils/logging.h"

#include <assert.h>
#include <errno.h>
#include <stddef.h>

using namespace std;
using namespace lcdn;
using namespace lcdn::base;
using namespace lcdn::net;

TCPClient::TCPClient(EventLoop* loop,
                     const InetAddress& server_addr,
                     const string& name,
                     TCPConnection::Delegate* delegate)
    : _loop(loop),
      _server_addr(server_addr),
      _name(name),
      _state(DISCONNECTED),
      _sockfd(-1),
      _channel(NULL),
      _watcher(this),
      _timer_watcher(this),
      _delegate(delegate),
      _last_id(0),
      _retry_delay_ms(INIT_RETRY_DELAY_MS),
      _is_retry(false),
      _is_connected(false),
      _connection(NULL)
{
}

TCPClient::~TCPClient()
{
}

void TCPClient::connect()
{
    /*LOG_INFO("TcpClient::connect[" << _name << "] - connecting to "
             << _server_addr.to_host_port());*/
    assert(_state == DISCONNECTED);
    _is_connected = true;
    _start();
}

void TCPClient::disconnect()
{
    _is_connected = false;
    if (_connection)
    {
        _connection->shutdown();
    }
}

void TCPClient::retry()
{
    _remove_connection();

    if (_is_retry && _is_connected)
    {
        _restart();
    }
}

void TCPClient::close(int connection_id)
{
    disconnect();
    _remove_connection();
}

void TCPClient::_new_connection(int sockfd)
{
    InetAddress peer_addr(sockets::get_peeraddr(sockfd));
    InetAddress local_addr(sockets::get_localaddr(sockfd));

    TCPConnection* connection =
        new TCPConnection(_loop, ++_last_id, sockfd, local_addr, peer_addr, _delegate);
    connection->_set_close_callback(_close_callback, this);

    _connection = connection;
    _connection->_connect_established();
}

void TCPClient::_remove_connection()
{
    LOG_DEBUG("TCPClient::_remove_connection");
    _connection->_connect_destroyed();
    if (_connection)
    {
        _connection_trash.push_back(_connection);
    }
}

void TCPClient::_start()
{
    assert(_state == DISCONNECTED);
    int sockfd = sockets::create_nonblocking_or_die();
    int ret = sockets::connect(sockfd, _server_addr.get_sockaddr_inet());
    int saved_errno = (ret == 0) ? 0 : errno;
    switch (saved_errno)
    {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
        _connecting(sockfd);
        break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
        _retry(sockfd);
        break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
        LOG_ERROR("connect error in Connector::startInLoop " << saved_errno);
        sockets::close(sockfd);
        break;

    default:
        LOG_ERROR("Unexpected error in Connector::startInLoop " << saved_errno);
        sockets::close(sockfd);
        // connectErrorCallback_();
        break;
    }
}

void TCPClient::_restart()
{
    _set_state(DISCONNECTED);
    _retry_delay_ms = INIT_RETRY_DELAY_MS;
    _start();
}

void TCPClient::_connecting(int sockfd)
{
    _set_state(CONNECTING);
    _channel = new Channel(sockfd);
    _channel->set_event_loop(_loop);
    _channel->set_watcher(&_watcher);
    _channel->enable_write();
}

int TCPClient::_remove_and_reset_channel()
{
    assert(NULL != _channel);
    _channel->disable_all();
    int sockfd = _channel->fd();
    if (_channel)
    {
        delete _channel;
        _channel = NULL;
    }
    return sockfd;
}

void TCPClient::_handle_write()
{
    assert(_state == CONNECTING);
    int sockfd = _remove_and_reset_channel();
    
    if (sockets::is_self_connect(sockfd))
    {
        LOG_WARN("Connector::handleWrite - Self connect");
        _retry(sockfd);
    }
    else
    {
        _set_state(CONNECTED);
        _new_connection(sockfd);
    }
}

void TCPClient::_retry(int sockfd)
{
    sockets::close(sockfd);
    _set_state(DISCONNECTED);
    LOG_INFO("Retry connecting to " << _server_addr.to_host_port()
             << " in " << _retry_delay_ms << " milliseconds. ");
    _loop->run_once(_retry_delay_ms, &_timer_watcher);
    int max_retry_delay_ms = MAX_RETRY_DEALY_MS;
    _retry_delay_ms = std::min(_retry_delay_ms * 2, max_retry_delay_ms);
}

// static
void TCPClient::_close_callback(void* arg, int id)
{
    TCPClient *client = static_cast<TCPClient*>(arg);
    client->_remove_connection();
}
