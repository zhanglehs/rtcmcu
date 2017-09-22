#include "echo.pb.h"
#include "tracker.pb.h"
#include "event_loop.h"
#include "rpc_channel.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

using namespace std;
using namespace lcdn;
using namespace lcdn::net;

void echo_done(echo::EchoResponse* resp) {
    printf("response: %s\n", resp->message().c_str());
}

class Timer1 : public EventLoop::TimerWatcher
{
public:
    Timer1(RPCPersistentChannel *channel){ _channel = channel; }
    ~Timer1(){}
    virtual void on_timer()
    {
        echo::EchoRequest request;
        request.set_message("hello");

        echo::EchoService::Stub stub(_channel);
        echo::EchoResponse* response = new echo::EchoResponse();
        stub.Echo(NULL, &request, response,
            gpb::NewCallback(::echo_done, response));

        tracker::F2TRegisterRequest request1;
        request1.set_ip(1);
        request1.set_port(1);
        request1.set_asn(2);
        request1.set_region(3);
        tracker::EchoService::Stub stub1(_channel);
        tracker::F2TRegisterResponse* response1 = new tracker::F2TRegisterResponse();
        stub1.Register(NULL, &request1, response1, NULL);
    }
private:
    RPCPersistentChannel* _channel;
};

int main()
{
    string client_name = "tcp_client";
    event_base *event_base = event_base_new();
    EventLoop loop(event_base);
    RPCPersistentChannel *channel = new RPCPersistentChannel(&loop, "10.10.69.127", 20345);
    channel->connect();
    Timer1* timer1 = new Timer1(channel);
    loop.run_forever(100, timer1);
    loop.loop();
    return 0;
}
