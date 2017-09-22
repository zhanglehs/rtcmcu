#ifndef _LCDN_NET_TCP_SERVER_H_
#define _LCDN_NET_TCP_SERVER_H_

#include <string>
#include <map>
#include <list>
#include "channel.h"
#include "socket.h"
#include "tcp_connection.h"

namespace lcdn
{
namespace net
{

class EventLoop;
class InetAddress;
class Socket;
class TCPConnection;

class TCPServer
{
    friend class AcceptorWatcher;

public:
    TCPServer(EventLoop* loop,
              const InetAddress& listen_addr,
              const std::string name,
              TCPConnection::Delegate* delegate);
    ~TCPServer();
    int start();
    const std::string& hostport() const { return _hostport; }
    const std::string& name() const { return _name; }
    EventLoop* event_loop() const { return _loop; }
    void close(int connection_id);

private:
    class AcceptorWatcher : public EventLoop::Watcher
    {
    public:
        explicit AcceptorWatcher(TCPServer* server) : _server(server) {}
        void on_read() { _server->_handle_accept(); }
        void on_write() {}
        virtual ~AcceptorWatcher() {}
    private:
        TCPServer* _server;
    };

    TCPConnection* _find_connection(int connection_id);
    void _new_connection(int sockfd, const InetAddress& peer_addr);
    void _handle_accept();

    static void _close_callback(void* arg, int id);

private:
    typedef std::map<int, TCPConnection*> IdToConnectionMap;
    typedef std::list<TCPConnection*> ConnectionTrashList;

    EventLoop* _loop;
    AcceptorWatcher _watcher;
    Socket _accept_socket;
    Channel _channel;
    const std::string _hostport;
    const std::string _name;

    TCPConnection::Delegate* const _delegate;

    int _last_id;
    IdToConnectionMap _id_to_connection;
    ConnectionTrashList _connection_trash;
};

} // namespace net
} // namespace lcdn

#endif // _LCDN_NET_TCP_SERVER_H_
