#ifndef _LCDN_NET_TCP_CONNECTION_H_
#define _LCDN_NET_TCP_CONNECTION_H_

#include "socket.h"
#include "channel.h"
#include "ip_endpoint.h"
#include "utils/buffer.hpp"
#include <string>

namespace lcdn
{
namespace net
{

class EventLoop;
class InetAddress;
class TCPServer;
class TCPClient;

class TCPConnection
{
    friend class TCPServer;
    friend class TCPClient;
    friend class ReadWriteWatcher;
public:
    class Delegate
    {
    public:
        virtual ~Delegate() {}
        virtual void on_connect(TCPConnection*) = 0;
        virtual void on_read(TCPConnection*) = 0;
        virtual void on_write_completed(TCPConnection*) = 0;
        virtual void on_close(TCPConnection*) = 0;
    };

    TCPConnection(EventLoop* loop,
                  int id,
                  int sockfd,
                  const InetAddress& local_addr,
                  const InetAddress& peer_addr,
                  TCPConnection::Delegate* const delegate);
    ~TCPConnection();
    const int id() const { return _id; }
    const EventLoop* event_loop() const { return _loop; }
    const InetAddress& local_address() { return _local_addr; }
    const InetAddress& peer_address() { return _peer_addr; }
    lcdn::base::Buffer* input_buffer() { return _input_buffer; }
    void* context() { return _context; }
    void set_context(void* context) { _context = context; }
    void set_input_buffer_size(int size) { _input_buffer_size = size; }
    void set_output_buffer_size(int size) { _output_buffer_size = size; }
    bool connected() const { return _state == CONNECTED; }

    int send(const void* data, size_t len);
    int send(lcdn::base::Buffer* buffer);
    void shutdown();

private:
    enum States { DISCONNECTED, CONNECTING, CONNECTED, DISCONNECTING };
    static const int DEFAULT_INPUT_BUFFER_SIZE = 2048;
    static const int DEFAULT_OUPUT_BUFFER_SIZE = 2048;

    typedef void(*CloseCallback)(void* arg, int id);
    class ReadWriteWatcher : public EventLoop::Watcher
    {
    public:
        explicit ReadWriteWatcher(TCPConnection* connection) : _connection(connection) {}
        void on_read() { _connection->_handle_read(); }
        void on_write() { _connection->_handle_write(); }
        virtual ~ReadWriteWatcher() {}
    private:
        TCPConnection* _connection;
    };

    void _set_state(States s) { _state = s; }
    void _set_close_callback(CloseCallback close_callback, void* arg) 
    { 
        _close_callback = close_callback; 
        close_callback_arg = arg;
    }
    bool _init();
    void _handle_read();
    void _handle_write();
    void _handle_close();
    int _do_read_fd(lcdn::base::Buffer* b, int fd);
    void _connect_established();
    void _connect_destroyed();
    int _send(const void* data, size_t len);
private:
    int _id;
    Socket _socket;
    Channel _channel;
    States _state;
    EventLoop* _loop;
    ReadWriteWatcher _watcher;
    InetAddress _local_addr;
    InetAddress _peer_addr;

    lcdn::base::Buffer* _input_buffer;
    int _input_buffer_size;
    lcdn::base::Buffer* _output_buffer;
    int _output_buffer_size;

    CloseCallback _close_callback;
    void* close_callback_arg;

    void* _context;
    TCPConnection::Delegate* const _delegate;
};

} // namespace net
} // namespace lcdn

#endif // _LCDN_NET_TCP_CONNECTION_H_
