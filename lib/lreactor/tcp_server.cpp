#include "tcp_server.h"
#include "sockets_ops.h"
#include "ip_endpoint.h"
#include "event_loop.h"
#include "tcp_connection.h"

#include "utils/logging.h"

#include <assert.h>
#include <stddef.h>

using namespace std;
using namespace lcdn;
using namespace lcdn::net;

class DeleteConnection : public EventLoop::Task
{
public:
    explicit DeleteConnection(TCPConnection* connection) 
        : _connection(connection) {}
    virtual ~DeleteConnection() {}
    virtual void run() { delete _connection; }
private:
    TCPConnection* _connection;
};

TCPServer::TCPServer(EventLoop* loop,
                     const InetAddress& listen_addr,
                     const string name,
                     TCPConnection::Delegate* delegate)
    : _loop(loop),
      _watcher(this),
      _last_id(0),
      _hostport(listen_addr.to_host_port()),
      _accept_socket(sockets::create_nonblocking_or_die()),
      _channel(_accept_socket.fd()),
      _delegate(delegate)
{
    log_initialize_file();

    LOG_INFO("tcp_server");

    _accept_socket.set_reuse_addr(true);
    _accept_socket.bind_address(listen_addr);
    _channel.set_event_loop(loop);
    _channel.set_watcher(&_watcher);
}

TCPServer::~TCPServer()
{
    for (IdToConnectionMap::iterator it(_id_to_connection.begin());
        it != _id_to_connection.end(); ++it)
    {
        TCPConnection* conn = it->second;
        delete conn;
    }
}

int TCPServer::start()
{
    _accept_socket.listen();
    _channel.enable_read();
    return 0;
}

void TCPServer::close(int connection_id)
{
    LOG_INFO("TCPServer::close(id=" << connection_id << ")");
    LOG_DEBUG("TCPServer::close");

    TCPConnection* connection = _find_connection(connection_id);
    if (NULL == connection) {
        LOG_INFO("TCPServer::close(id=" << connection_id << ") connection==NULL");
        return;
    }

    connection->_connect_destroyed();
    _id_to_connection.erase(connection_id);

    // The call stack might have callbacks which still have the pointer of
    // connection. Instead of referencing connection with ID all the time,
    // destroys the connection in next run loop to make sure any pending
    // callbacks in the call stack return.
    _loop->post_task(new DeleteConnection(connection));
    LOG_INFO("TCPServer::close(id=" << connection_id << ") exit");
}

TCPConnection* TCPServer::_find_connection(int connection_id)
{
    IdToConnectionMap::iterator it = _id_to_connection.find(connection_id);
    if (it == _id_to_connection.end())
        return NULL;
    return it->second;
}

void TCPServer::_new_connection(int sockfd, const InetAddress& peer_addr)
{
    InetAddress local_addr(sockets::get_localaddr(sockfd));
    TCPConnection* connection =
        new TCPConnection(_loop, ++_last_id, sockfd, local_addr, peer_addr, _delegate);
    _id_to_connection[connection->id()] = connection;
    connection->_set_close_callback(_close_callback, this);
    connection->_connect_established();
}

void TCPServer::_handle_accept()
{
    InetAddress peer_addr(0);
    int connfd = _accept_socket.accept(&peer_addr);
    
    if (connfd >= 0)
    {
        _new_connection(connfd, peer_addr);
    }
}

// static 
void TCPServer::_close_callback(void* arg, int id)
{
    LOG_DEBUG("TCPServer::_close_callback");

    TCPServer* server = static_cast<TCPServer*>(arg);
    server->close(id);
}
