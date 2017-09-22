#include "assert.h"
#include "event.h"
#include "event_loop.h"
#include "ip_endpoint.h"
#include "cmd_server.h"
#include "rpc_server.h"
#include "echo.pb.h"
#include "tracker.pb.h"

using namespace lcdn;
using namespace lcdn::net;

class EchoServiceImpl : public echo::EchoService
{
public:
    EchoServiceImpl()
    {
    }

    virtual void Echo(::google::protobuf::RpcController* controller,
                      const ::echo::EchoRequest* request,
                      ::echo::EchoResponse* response,
                      ::google::protobuf::Closure* done)
    {
        response->set_message(request->message());

        if (done)
        {
            done->Run();
        }
    }
};

class TrackerServiceImpl : public tracker::EchoService
{
public:
    TrackerServiceImpl()
    {
    }

    virtual void Register(::google::protobuf::RpcController* controller,
                          const ::tracker::F2TRegisterRequest* request,
                          ::tracker::F2TRegisterResponse* response,
                          ::google::protobuf::Closure* done)
    {
        //printf("register ip: %d, port: %d, asn: %d, region: %d\n", request->ip(), request->port(), request->asn(), request->region());
        response->set_result(0);

        if (done)
        {
            done->Run();
        }
    }

    virtual void GetAddr(::google::protobuf::RpcController* controller,
                         const ::tracker::F2TAddrRequest* request,
                         ::tracker::F2TAddrResponse* response,
                         ::google::protobuf::Closure* done)
    {
        printf("get addr\n");

        if (done)
        {
            done->Run();
        }
    }

    virtual void UpdateStream(::google::protobuf::RpcController* controller,
                              const ::tracker::F2TUpdateStreamRequest* request,
                              ::tracker::F2TUpdateStreamResponse* response,
                              ::google::protobuf::Closure* done)
    {
        printf("update stream\n");

        if (done)
        {
            done->Run();
        }
    }
};

int main(int argc, char* argv[])
{
    InetAddress net_addr(20345);
    // create event loop
    struct event_base * main_base = event_base_new();
    EventLoop event_loop(main_base);
    // create cmd server
    CMDServer cmd_server(&event_loop,
                          net_addr,
                          "cmd_server");
    RPCMgr* mgr = new RPCMgr();
    mgr->register_service(new EchoServiceImpl());
    mgr->register_service(new TrackerServiceImpl());
    cmd_server.register_role_mgr(mgr);

    // start cmd_server, listening
    cmd_server.start();
    event_loop.loop();

    // release player_fsm_mgr; uploader_fsm_mgr; fsm_mgr
    return 0;
}
