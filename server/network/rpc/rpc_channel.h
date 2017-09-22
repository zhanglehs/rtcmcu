#ifndef _LCDN_NET_RPC_CHANNEL_H_
#define _LCDN_NET_RPC_CHANNEL_H_

#include <string>
#include <google/protobuf/service.h>



namespace lcdn
{
namespace net
{
namespace gpb = google::protobuf;
class EventLoop;

class RPCPersistentChannel : public gpb::RpcChannel
{
public:
    RPCPersistentChannel(EventLoop* loop, const std::string& host, int port);

    virtual ~RPCPersistentChannel();

    void connect();

    void close();

    // overwrite
    virtual void CallMethod(const gpb::MethodDescriptor* method,
                            gpb::RpcController* controller,
                            const gpb::Message* request,
                            gpb::Message* response,
                            gpb::Closure* done);
private:
    class Impl;
    Impl *_impl;
};

} // namespace net
} // namespace lcdn

#endif // _LCDN_NET_RPC_CHANNEL_H_
