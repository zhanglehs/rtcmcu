#include "rpc_channel.h"
#include "rpc_common.h"
#include "tcp_client.h"
#include "event_loop.h"
#include "ip_endpoint.h"
#include "tcp_connection.h"
#include "utils/buffer.hpp"

#include <string>
#include <list>
#include <map>
#include <stddef.h>
#include <netinet/in.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

using namespace std;
using namespace lcdn;
using namespace lcdn::base;
using namespace lcdn::net;

static const uint32_t METHOD_TIMEOUT = 5; // second
static const uint32_t KEEPALIVED_TIMEOUT = 5;

static const uint32_t METHOD_TIMEOUT_TIMER_PERIOD = 3000; //ms
static const uint32_t KEEPALIVED_TIMER_PERIOD = 5000;
static const uint32_t CHECK_KEEPALIVED_TIMER_PERIOD = 10000;

struct MessageResponse
{
    MessageResponse()
        : controller(NULL),
          response(NULL),
          done(NULL),
          action_time(0) {}

    gpb::RpcController* controller;
    gpb::Message* response;
    gpb::Closure* done;
    int64_t action_time;
};

class RPCPersistentChannel::Impl : public TCPConnection::Delegate
{
public:
    class TimeoutTimerWatcher : public EventLoop::TimerWatcher
    {
    public:
        explicit TimeoutTimerWatcher(RPCPersistentChannel::Impl* impl)
            : _impl(impl) {}
        virtual void on_timer() { _impl->remove_timeout_method(); }
        ~TimeoutTimerWatcher() {}
    private:
        RPCPersistentChannel::Impl* _impl;
    };

    class CheckKeepalivedTimerWatcher : public EventLoop::TimerWatcher
    {
    public:
        explicit CheckKeepalivedTimerWatcher(RPCPersistentChannel::Impl* impl)
            : _impl(impl) {}
        virtual void on_timer() { _impl->check_keepalived(); }
        ~CheckKeepalivedTimerWatcher() {}
    private:
        RPCPersistentChannel::Impl* _impl;
    };

    class KeepalivedTimerWatcher : public EventLoop::TimerWatcher
    {
    public:
        explicit KeepalivedTimerWatcher(RPCPersistentChannel::Impl* impl)
            : _impl(impl) {}
        virtual void on_timer() { _impl->send_keepalived(); }
        ~KeepalivedTimerWatcher() {}
    private:
        RPCPersistentChannel::Impl* _impl;
    };

    Impl(EventLoop* loop, const InetAddress& server_addr, const string& name);
    ~Impl();

    void CallMethod(const gpb::MethodDescriptor* method,
                    gpb::RpcController* controller,
                    const gpb::Message* request,
                    gpb::Message* response,
                    gpb::Closure* done);

    virtual void on_connect(TCPConnection*) {}
    virtual void on_read(TCPConnection*);
    virtual void on_write_completed(TCPConnection*) {}
    virtual void on_close(TCPConnection*) {}

    MessageResponse* get_message_response();
    void free_message_response(MessageResponse *response);
    void send_packet(uint32_t seq, uint32_t opcode, const ::google::protobuf::Message *message);
    void handle_packet(const rpc_rsp_header& header, lcdn::base::Buffer* payload, size_t payload_len);
    void serialize_from_message(const gpb::Message* msg, Buffer* buf);
    bool deserialize_to_message(gpb::Message* msg, Buffer* buf, size_t payload_len);
    void remove_timeout_method();
    void send_keepalived();
    void check_keepalived();

public:
    typedef list<MessageResponse*> MessageResponseList;
    typedef map<uint32_t, MessageResponse*> MessageResponseMap;
    MessageResponseMap _message_response_map;
    MessageResponseList _free_response_list;
    TCPClient _client;
    uint32_t _seq;
    Buffer* _out_buffer;

    TimeoutTimerWatcher _timeout_timer_watcher;
    KeepalivedTimerWatcher _keepalived_timer_watcher;
    CheckKeepalivedTimerWatcher _check_keepalived_timer_watcher;

    int64_t _last_keepalived_time;
};

RPCPersistentChannel::Impl::Impl(EventLoop* loop, 
                                 const InetAddress& server_addr,
                                 const string& name)
    : _client(loop, server_addr, name, this),
      _seq(0),
      _out_buffer(NULL),
      _timeout_timer_watcher(this),
      _keepalived_timer_watcher(this),
      _check_keepalived_timer_watcher(this),
      _last_keepalived_time(time(0))
{
    _out_buffer = new Buffer(MESSAGE_MAX_LENGTH + 16);
    _client.enable_retry();

    loop->run_forever(METHOD_TIMEOUT_TIMER_PERIOD, &_timeout_timer_watcher);
    loop->run_forever(KEEPALIVED_TIMER_PERIOD, &_keepalived_timer_watcher);
    loop->run_forever(CHECK_KEEPALIVED_TIMER_PERIOD, &_check_keepalived_timer_watcher);
}

RPCPersistentChannel::Impl::~Impl()
{
    if (_out_buffer)
    {
        delete _out_buffer;
        _out_buffer = NULL;
    }
}

void RPCPersistentChannel::Impl::CallMethod(const gpb::MethodDescriptor* method,
                                            gpb::RpcController* controller,
                                            const gpb::Message* request,
                                            gpb::Message* response,
                                            gpb::Closure* done)
{
    assert(NULL != method);
    assert(NULL != request);
    assert(NULL != response);
    MessageResponse *message_response = get_message_response();
    message_response->controller = controller;
    message_response->response = response;
    message_response->done = done;
    message_response->action_time = time(0);
    uint32_t opcode = hash_string(method->full_name());
    gpb::Message* save_request = request->New();
    if (!save_request->IsInitialized()) 
    {
        save_request->CopyFrom(*request);
    }
    /*VLOG_INFO() << "call service: " << method->full_name()
        << ", opcode: " << opcode
        << ", request: " << save_request->DebugString();*/
    _message_response_map[_seq] = message_response;
    send_packet(_seq++, opcode, save_request);
    delete save_request;
}

bool parse_cmd_header(Buffer* buf, proto_header& header)
{
    assert(NULL != buf);

    if (buf->data_len() < sizeof(proto_header))
    {
        return false;
    }

    proto_header *ih = (proto_header*)(buf->data_ptr());

    header.magic = ih->magic;
    header.version = ih->version;
    header.cmd = ntohs(ih->cmd);
    header.size = ntohl(ih->size);

    return true;
}

bool finished_frame(Buffer* buf, const proto_header& header)
{
    assert(NULL != buf);

    if (header.size > buf->data_len())
    {
        return false;
    }

    return true;
}

void decode_rpc_header(Buffer* payload, rpc_rsp_header& rpc_header)
{
    rpc_rsp_header* header = (rpc_rsp_header*)(payload->data_ptr());
    assert(NULL != header);

    rpc_header.opcode = header->opcode;
    rpc_header.seq = header->seq;
}

void RPCPersistentChannel::Impl::on_read(TCPConnection* connection)
{
    assert(NULL != connection);
    Buffer* buf = connection->input_buffer();

    do
    {
        bool parse_success_flag = false;
        bool is_frame_flag = false;
        proto_header header;

        parse_success_flag = parse_cmd_header(buf, header);

        if (parse_success_flag)
        {
            is_frame_flag = finished_frame(buf, header);
            if (is_frame_flag)
            {
                buf->eat(sizeof(proto_header));

                if (CMD_RPC_KEEPALIVE_STATE == header.cmd)
                {
                    _last_keepalived_time = time(0);
                }
                else
                {
                    rpc_rsp_header rpc_header;
                    decode_rpc_header(buf, rpc_header);
                    size_t len = buf->eat(sizeof(rpc_rsp_header));
                    assert(len == sizeof(rpc_rsp_header));
                    size_t payload_len = header.size - sizeof(proto_header) - sizeof(rpc_rsp_header);
                    handle_packet(rpc_header, buf, payload_len);
                    buf->eat(payload_len);
                }
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    } while (true);

    buf->adjust();
}

MessageResponse* RPCPersistentChannel::Impl::get_message_response()
{
    if (_free_response_list.empty())
    {
        return new MessageResponse();
    }
    MessageResponse *message_response = _free_response_list.front();
    _free_response_list.pop_front();
    return message_response;
}

void RPCPersistentChannel::Impl::free_message_response(MessageResponse *response)
{
    if (response->response)
    {
        delete response->response;
        response->response = NULL;
    }

    if (response->controller)
    {
        delete response->controller;
        response->controller = NULL;
    }

    _free_response_list.push_back(response);
}

void RPCPersistentChannel::Impl::send_packet(uint32_t seq, 
                                             uint32_t opcode, 
                                             const ::google::protobuf::Message *message)
{
    _out_buffer->reset();

    rpc_req_header rpc_header;
    rpc_header.seq = seq;
    rpc_header.opcode = opcode;
    uint32_t length = message->ByteSize();
    encode_rpc_req_header(&rpc_header, _out_buffer, length);
    serialize_from_message(message, _out_buffer);
    _client.connection()->send(_out_buffer);
}

void RPCPersistentChannel::Impl::handle_packet(
    const rpc_rsp_header& header, lcdn::base::Buffer* payload, size_t payload_len)
{
    uint32_t seq = header.seq;
    MessageResponseMap::iterator it;
    if ((it = _message_response_map.find(seq)) != _message_response_map.end())
    {
        MessageResponse* message_response = _message_response_map[seq];
        deserialize_to_message(message_response->response, payload, payload_len);

        if (message_response->done)
        {
            message_response->done->Run();
        }

        _message_response_map.erase(seq);
        free_message_response(message_response);
    }
}

void RPCPersistentChannel::Impl::serialize_from_message(const gpb::Message* msg, Buffer* buf)
{
    char temp[MESSAGE_MAX_LENGTH];
    msg->SerializeToArray(temp, MESSAGE_MAX_LENGTH);
    buf->append_ptr(temp, msg->ByteSize());
}

bool RPCPersistentChannel::Impl::deserialize_to_message(gpb::Message* msg, Buffer* buf, size_t payload_len)
{
    bool result = msg->ParseFromArray(buf->data_ptr(), static_cast<int>(payload_len));

    if (!result)
    {
        // TODO log
    }

    return result;
}

void RPCPersistentChannel::Impl::remove_timeout_method()
{
    MessageResponseMap::iterator it = _message_response_map.begin();

    time_t now = time(0);
    while (it != _message_response_map.end())
    {
        if (now - it->second->action_time > METHOD_TIMEOUT)
        {
            printf("timeout %d\n", it->first);
            MessageResponse* message_response = it->second;
            _message_response_map.erase(it++);
            free_message_response(message_response);
        }
        else { it++; }
    }
}

void RPCPersistentChannel::Impl::check_keepalived()
{
    if (time(0) - _last_keepalived_time > KEEPALIVED_TIMEOUT)
    {
        _client.retry();
        // TODO log
    }
}

void RPCPersistentChannel::Impl::send_keepalived()
{
    Buffer buf(sizeof(proto_header));
    encode_header(&buf, CMD_RPC_KEEPALIVE_STATE, sizeof(proto_header));
    _client.connection()->send(&buf);
}

RPCPersistentChannel::RPCPersistentChannel(EventLoop* loop,
                                           const string& host, 
                                           int port)
{
    InetAddress server_addr(host, port);
    _impl = new Impl(loop, server_addr, "rpc_channel");
}

RPCPersistentChannel::~RPCPersistentChannel()
{
    delete _impl;
}

void RPCPersistentChannel::connect()
{
    _impl->_client.connect();
}

void RPCPersistentChannel::close()
{
    //_impl->_client.close();
}

void RPCPersistentChannel::CallMethod(const gpb::MethodDescriptor* method,
                                      gpb::RpcController* controller,
                                      const gpb::Message* request,
                                      gpb::Message* response,
                                      gpb::Closure* done)
{
    _impl->CallMethod(method, controller, request, response, done);
}

