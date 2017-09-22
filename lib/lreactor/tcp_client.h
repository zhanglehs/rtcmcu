#ifndef _LCDN_NET_CLIENT_H_
#define _LCDN_NET_CLIENT_H_

#include <string>
#include <list>
#include "ip_endpoint.h"
#include "socket.h"
#include "channel.h"
#include "tcp_connection.h"

namespace lcdn
{
namespace net
{

class EventLoop;
class TCPConnection;

class TCPClient
{
public:
    TCPClient(EventLoop* loop,
              const InetAddress& server_addr,
              const std::string& name,
              TCPConnection::Delegate* delegate);
    ~TCPClient();

    void connect();
    void disconnect();
    void retry();
    void close(int connection_id);

    void enable_retry() { _is_retry = true; }
    const InetAddress& server_address() const { return _server_addr; }
    TCPConnection* connection() { return _connection; }
    EventLoop* event_loop() const { return _loop; }
private:
    class ConnectorWatcher : public EventLoop::Watcher
    {
    public:
        explicit ConnectorWatcher(TCPClient* client) : _client(client) {}
        void on_read() {}
        void on_write() { _client->_handle_write(); }
        virtual ~ConnectorWatcher() {}
    private:
        TCPClient* _client;
    };

    typedef std::list<TCPConnection*> ConnectionTrashList;
    class TCPClientTimerWatcher : public EventLoop::TimerWatcher
    {
    public:
        explicit TCPClientTimerWatcher(TCPClient* client) : _client(client) {}
        virtual void on_timer() { _client->_start(); }
        ~TCPClientTimerWatcher() {}
    private:
        TCPClient* _client;
    };

    enum States { DISCONNECTED, CONNECTING, CONNECTED };
    static const int MAX_RETRY_DEALY_MS = 30 * 1000;
    static const int INIT_RETRY_DELAY_MS = 500;

    void _new_connection(int sockfd);
    void _remove_connection();
    void _handle_write();

    void _start();
    void _restart();
    void _connecting(int sockfd);
    int _remove_and_reset_channel();
    void _retry(int sockfd);
    void _set_state(States s) { _state = s; }

    static void _close_callback(void* arg, int id);
private:
    EventLoop* _loop;
    const InetAddress _server_addr;
    const std::string _name;

    States _state;
    int _sockfd;
    Channel* _channel;
    ConnectorWatcher _watcher;
    TCPClientTimerWatcher _timer_watcher;
    TCPConnection::Delegate* const _delegate;
    
    int _last_id;
    int _retry_delay_ms;
    bool _is_retry;
    bool _is_connected;
    TCPConnection* _connection;

    ConnectionTrashList _connection_trash;
};

} // namespace net
} // namespace lcdn

#endif // _LCDN_NET_CLIENT_H_
