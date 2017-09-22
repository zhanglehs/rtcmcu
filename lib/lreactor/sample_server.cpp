#include "tcp_server.h"
#include "tcp_connection.h"
#include "event_loop.h"
#include "ip_endpoint.h"
#include "utils/logging.h"

#include <stddef.h>
#include <string>
#include <event.h>

using namespace std;
using namespace lcdn;
using namespace lcdn::net;

class SampleServer : public TCPConnection::Delegate
{
public:
    SampleServer(EventLoop* loop, InetAddress& listen_addr, string& name);
    ~SampleServer();
    void start();
    virtual void on_connect(TCPConnection*){}
    virtual void on_read(TCPConnection*);
    virtual void on_write_completed(TCPConnection*){}
    virtual void on_close(TCPConnection*){}

private:
    EventLoop *_loop;
    TCPServer _tcp_server;
};

SampleServer::SampleServer(EventLoop* loop, InetAddress& listen_addr, string& name)
    : _tcp_server(loop, listen_addr, name, this)
{
}

SampleServer::~SampleServer()
{}

void SampleServer::start()
{
    _tcp_server.start();
}

void SampleServer::on_read(TCPConnection* connection)
{
    LOG_INFO("connection id " << connection->id());
    string str = "HTTP/1.1 400 Bad Request\r\n\r\n";
    connection->send(str.c_str(), str.length());
    //connection->shutdown();
}

int main()
{
    log_initialize_default();
    string server_name = "tcp_server";
    event_base *event_base = event_base_new();
    EventLoop loop(event_base);
    
    InetAddress listen_addr(10345);
    SampleServer server(&loop, listen_addr, server_name);
    server.start();
    loop.loop();
    return 0;
}
