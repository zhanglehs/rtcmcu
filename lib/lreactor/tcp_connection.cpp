#include "tcp_connection.h"
#include "event_loop.h"
#include "ip_endpoint.h"
#include "tcp_server.h"
#include "net_errors.h"
#include "utils/logging.h"

#include <errno.h>
#include <assert.h>
#include <stddef.h>
#include <sys/ioctl.h>

using namespace lcdn;
using namespace lcdn::base;
using namespace lcdn::net;

#define MAX_READ_SIZE_ONCE 2048

TCPConnection::TCPConnection(EventLoop* loop,
                             int id,
                             int sockfd,
                             const InetAddress& local_addr,
                             const InetAddress& peer_addr,
                             TCPConnection::Delegate* const delegate)
    : _id(id),
      _socket(sockfd),
      _channel(sockfd),
      _state(CONNECTING),
      _loop(loop),
      _watcher(this),
      _local_addr(local_addr),
      _peer_addr(peer_addr),
      _input_buffer(NULL),
      _input_buffer_size(DEFAULT_INPUT_BUFFER_SIZE),
      _output_buffer(NULL),
      _output_buffer_size(DEFAULT_OUPUT_BUFFER_SIZE),
      _context(NULL),
      _delegate(delegate)
{
    LOG_INFO("TCPConnection ctor() id:" << _id << " fd:" << _socket.fd());
    _channel.set_event_loop(loop);
    _channel.set_watcher(&_watcher);
    _socket.set_tcp_nodelay(true);
}

TCPConnection::~TCPConnection()
{
    LOG_INFO("TCPConnection dtor() id:" << _id << " fd:" << _socket.fd());
}

int TCPConnection::send(const void* data, size_t len)
{
    if (_state == CONNECTED)
    {
        return _send(data, len);
    }

    return ERR_CONNECTION_CLOSED;
}

int TCPConnection::send(Buffer* buffer)
{
    if (_state == CONNECTED)
    {
        return _send(buffer->data_ptr(), buffer->data_len());
    }
    LOG_ERROR("tcpconnection::send: _state isnot connected!");
    return ERR_CONNECTION_CLOSED;
}

void TCPConnection::shutdown()
{
    LOG_INFO("TCPConnection::shutdown() id:" << _id << " fd:" << _socket.fd() << " state:" << _state);
    LOG_DEBUG("shutdown: connection shutdown, peer_addr: " << _peer_addr.to_host_port()<<" local_addr: "<<_local_addr.to_host_port());

    if (_state == CONNECTED)
    {
        LOG_INFO("shutdown _state: CONNECTED");

        _set_state(DISCONNECTING);
        if (!_channel.is_writing())
        {
            LOG_INFO("shutdown shutdown_write");
            // we are not writing
            _socket.shutdown_write();
        }
    }

    LOG_INFO("TCPConnection::shutdown() exit id:" << _id << " fd:" << _socket.fd() << " state:" << _state);
}

bool TCPConnection::_init()
{
    _input_buffer = new Buffer(static_cast<uint32_t>(_input_buffer_size));
    if (NULL == _input_buffer)
        return false;
    _output_buffer = new Buffer(static_cast<uint32_t>(_output_buffer_size));
    if (NULL == _output_buffer)
        return false;
    return true;
}

void TCPConnection::_connect_established()
{
    LOG_INFO("TCPConnection::_connect_established id:" << _id << " fd:" << _socket.fd() << " state:" << _state);
    assert(_state == CONNECTING);
    _set_state(CONNECTED);
    _channel.enable_read();
    _delegate->on_connect(this);
    
    if (!_init())
    {
        LOG_DEBUG("_connect_established: connection shutdown, peer_addr: " << _peer_addr.to_host_port()<<" local_addr: "<<_local_addr.to_host_port());
        _handle_close();
    }

    LOG_INFO("TCPConnection::_connect_established exit id:" << _id << " fd:" << _socket.fd() << " state:" << _state);
}

void TCPConnection::_connect_destroyed()
{
    LOG_INFO("TCPConnection::_connect_destroyed id:" << _id << " fd:" << _socket.fd() << " state:" << _state);
    LOG_DEBUG("_connect_destroyed:  connection shutdown, _peer_addr: " << _peer_addr.to_host_port()<<" _local_addr: "<<_local_addr.to_host_port());
    if (_state == CONNECTED)
    {
        _set_state(DISCONNECTED);
        _channel.disable_all();
        _delegate->on_close(this);
    }

    if (_input_buffer)
    {
        delete _input_buffer;
        _input_buffer = NULL;
    }

    if (_output_buffer)
    {
        delete _output_buffer;
        _output_buffer = NULL;
    }

    _loop = NULL;
    _context = NULL;
    LOG_INFO("TCPConnection::_connect_destroyed exit id:" << _id << " fd:" << _socket.fd() << " state:" << _state);
}

void TCPConnection::_handle_read()
{
    int n = _do_read_fd(_input_buffer, _channel.fd());

    if (n > 0)
    {
        _delegate->on_read(this);
    }
    else if (n < 0)
    {
        LOG_DEBUG("_handle_read: connection close, peer_addr: " << _peer_addr.to_host_port()<<" local_addr: "<<_local_addr.to_host_port());
        _handle_close();
    }
    else
    {
        // need retry
    }
}

void TCPConnection::_handle_write()
{
    if (_channel.is_writing())
    {
        ssize_t n = ::write(_channel.fd(), _output_buffer->data_ptr(), _output_buffer->data_len());
        if (n >= 0)
        {
            _output_buffer->ignore(n);
            if (_output_buffer->data_len() == 0)
            {
                _channel.disable_write();
                _delegate->on_write_completed(this);
                if (_state == DISCONNECTING)
                {
                    shutdown();
                }
            }
            else
            {
                LOG_TRACE("I am going to write more data");
            }

            // debug info
            if (n == 0)
            {
                LOG_DEBUG("_handle_write: write return 0");
            }
        }
        else
        {
            if (n == -1 && (errno != EINTR && errno != EAGAIN))
            {
                LOG_DEBUG("_handle_write: connection close, peer_addr: " << _peer_addr.to_host_port()<<" local_addr: "<<_local_addr.to_host_port());
                _handle_close();
            }
        }
    }
}

void TCPConnection::_handle_close()
{
    LOG_INFO("TCPConnection::_handle_close() id:" << _id << " fd:" << _socket.fd() << " state:" << _state);
    if (_close_callback)
        _close_callback(close_callback_arg, _id);
    else
        LOG_INFO("not setting close callback, this may raise memory leak!");
    LOG_INFO("TCPConnection::_handle_close() exit id:" << _id << " fd:" << _socket.fd() << " state:" << _state);
}

int TCPConnection::_do_read_fd(Buffer* b, int fd)
{
    int toread = 0, r;

    if (fd <= 0 || b == NULL)
    {
        LOG_INFO("_do_read_fd: fd<=0 || b==NULL");
        return -1;
    }
    if (ioctl(fd, FIONREAD, &toread))
    {
        if (errno != EAGAIN && errno != EINTR)
        {
            LOG_INFO("_do_read_fd: errno: " << errno);
            return -1;
        }
        else
            return 0;
    }

    /* try to detect whether it's eof */
    if (toread == 0)
        toread = 1;
    toread = toread < MAX_READ_SIZE_ONCE ? toread : MAX_READ_SIZE_ONCE;
    toread = toread < (int)b->capacity() ? toread : (int)b->capacity();

    if (toread == 0)
    {
        return 0;
    }
    char buffer[MAX_READ_SIZE_ONCE];
    r = read(fd, buffer, toread);
    if ((r == -1 && (errno != EINTR && errno != EAGAIN)) || r == 0)
    {
        // connection closed or reset
        if (r == 0)
        {
            //WRN("read 0 bytes");
        }
        LOG_INFO("_do_read_fd: errno: " << errno << strerror(errno)<<"r: "<<r);
        return -1;
    }
    if (r > 0)
    {
        b->append_ptr(buffer, r);
        return r;
    }
    else
    {
        return 0;
    }
}

int TCPConnection::_send(const void* data, size_t len)
{
    if (_output_buffer->capacity() < len)
    {
        LOG_ERROR("_send: err_buffer_size_low");    
        return ERR_BUFFER_SIZE_LOW;
    }

    ssize_t nwrote = 0;
    // if nothing in output queue, try writing directly
    if (!_channel.is_writing() && _output_buffer->data_len() == 0)
    {
        nwrote = ::write(_channel.fd(), data, len);
        if (nwrote >= 0)
        {
            if (static_cast<size_t>(nwrote) == len)
            {
                _delegate->on_write_completed(this);
            }

            // debug info
            if (nwrote == 0)
            {
                LOG_DEBUG("_send: write return 0");
            }
        }
        else // nwrote < 0
        {
            if (nwrote == -1 && (errno != EINTR && errno != EAGAIN))
            {
                LOG_ERROR("_send: Tcp connection output buffer is not big enough, connection close, \
                    peer_addr: " << _peer_addr.to_host_port()<<" local_addr: "<<_local_addr.to_host_port());
                _handle_close();
                return ERR_CONNECTION_CLOSED;
            }

            LOG_DEBUG("_send: nwrote < 0");
            nwrote = 0;
        }
    }

    assert(nwrote >= 0);
    if (static_cast<size_t>(nwrote) < len)
    {
        if (_output_buffer->append_ptr(static_cast<const char*>(data)+nwrote, len - nwrote) > 0)
        {
            LOG_TRACE("_send: append_ptr success!");
            if (!_channel.is_writing())
            {
                LOG_TRACE("_send: channel isnot writing");
                _channel.enable_write();
            }
        }
        else
        {
            LOG_ERROR("_send: Tcp connection output buffer is not big enough, \
                    peer_addr: " << _peer_addr.to_host_port()<<" local_addr: "<<_local_addr.to_host_port());
            // _handle_close();
            return ERR_BUFFER_SIZE_LOW;
        }
    }
    return OK;
}
