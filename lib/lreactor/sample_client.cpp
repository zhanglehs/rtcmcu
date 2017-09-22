#include "tcp_client.h"
#include "tcp_connection.h"
#include "event_loop.h"
#include "ip_endpoint.h"

#include <stddef.h>
#include <string>
#include <event.h>

using namespace std;
using namespace lcdn;
using namespace lcdn::net;

class SampleClient : public TCPConnection::Delegate,
                     public EventLoop::TimerWatcher
{
public:
    SampleClient(EventLoop* loop, InetAddress& listen_addr, string& name);
    ~SampleClient();
    void start();
    virtual void on_connect(TCPConnection*){}
    virtual void on_read(TCPConnection*);
    virtual void on_write_completed(TCPConnection*);
    virtual void on_close(TCPConnection*){}
    virtual void on_timer();
private:
    EventLoop *_loop;
    TCPClient _tcp_client;
};

SampleClient::SampleClient(EventLoop* loop, InetAddress& listen_addr, string& name)
    : _tcp_client(loop, listen_addr, name, this)
{
}

SampleClient::~SampleClient()
{}

void SampleClient::start()
{
    _tcp_client.connect();
}

void SampleClient::on_read(TCPConnection* connection)
{
}

void SampleClient::on_write_completed(TCPConnection* connection)
{
}

void SampleClient::on_timer()
{
    string str = "echo";
    _tcp_client.connection()->send(str.c_str(), str.length());
}

int main()
{
    string client_name = "tcp_client";
    event_base *event_base = event_base_new();
    EventLoop loop(event_base);
    
    InetAddress server_addr("10.10.69.127", 10345);
    SampleClient client(&loop, server_addr, client_name);
    client.start();
    loop.run_forever(1000, &client);
    loop.loop();
    return 0;
}
