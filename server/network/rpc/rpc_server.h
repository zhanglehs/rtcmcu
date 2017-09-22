#ifndef _LCDN_NET_RPC_SERVER_H_
#define _LCDN_NET_RPC_SERVER_H_

#include "cmd_fsm.h"
#include "cmd_fsm_state.h"
#include "cmd_base_role_mgr.h"
#include "rpc_common.h"

#include <map>
#include <google/protobuf/service.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/message.h>

namespace lcdn
{
namespace net
{

namespace gpb = google::protobuf;

struct RPCMethod
{
    RPCMethod(gpb::Service* service,
              const gpb::Message* request,
              const gpb::Message* response,
              const gpb::MethodDescriptor* method)
        : _service(service),
          _request(request),
          _response(response),
          _method(method)
    {
    }

    gpb::Service* _service;
    const gpb::Message* _request;
    const gpb::Message* _response;
    const gpb::MethodDescriptor* _method;
};

struct HandleServiceEntry 
{
public:
    HandleServiceEntry(uint32_t opcode,
                         uint32_t seq,
                         gpb::Message* request, 
                         gpb::Message* response, 
                         CMDFSM* fsm)
    : _opcode(opcode),
      _seq(seq),
      _request(request), 
      _response(response), 
      _fsm(fsm)
    {
    }
    uint32_t _opcode;
    uint32_t _seq;
    gpb::Message* _request;
    gpb::Message* _response;
    CMDFSM* _fsm;
};

class RPCMethodManager
{
public:
    typedef std::map<uint32_t, RPCMethod*> RPCMethodMap;

    RPCMethodManager()
    {}

    ~RPCMethodManager()
    {
        RPCMethodMap::iterator iter = _rpc_method_map.begin();
        for(; iter != _rpc_method_map.end();)
        {
            RPCMethod* rpc_method = iter->second;
            ++iter;
            delete rpc_method;
        }
    }

    void register_service(gpb::Service* service);
    bool handle_packet(const rpc_req_header& rpc_header, const char* payload, 
                       size_t payload_len, CMDFSM* fsm);
private:
    RPCMethodMap _rpc_method_map;
};

class RPCConnection
{
public:
    RPCConnection()
        : _fsm(NULL),
          _method_manager(NULL)
    {}

    void set_fsm(CMDFSM* fsm)
    {
        _fsm = fsm;
    }

    CMDFSM* fsm() { return _fsm; }

    void set_role_info(RoleInfo& role_info)
    {
        _role_info = role_info;
    }

    void set_method_manager(RPCMethodManager* method_manager)
    {
        _method_manager = method_manager;
    }

    void handle_packet(lcdn::base::Buffer* payload, size_t payload_len);
    void decode_rpc_header(lcdn::base::Buffer* payload, rpc_req_header& rpc_header);
private:
    CMDFSM* _fsm;
    RoleInfo _role_info;
    RPCMethodManager* _method_manager;
};

class CMDFSMStateRecvPkt : public CMDFSMState
{
public:
    CMDFSMStateRecvPkt() {}
    virtual ~CMDFSMStateRecvPkt() {}

    virtual int handle_read(CMDFSM* fsm, const proto_header& header, lcdn::base::Buffer* payload)
    {
        RPCConnection* rpc_connection = static_cast<RPCConnection*>(fsm->context());

        if (CMD_RPC_KEEPALIVE_STATE == header.cmd)
        {
            lcdn::base::Buffer buf(sizeof(proto_header));
            encode_header(&buf, CMD_RPC_KEEPALIVE_STATE, sizeof(proto_header));
            rpc_connection->fsm()->send(&buf);
        }
        else
        {
            rpc_connection->handle_packet(payload, header.size - sizeof(proto_header));
        }
        return 0;
    }

    virtual int handle_write(CMDFSM* fsm, const proto_header& header, lcdn::base::Buffer* payload)
    {
        return 0;
    }

};

class RPCMgr : public CMDBaseRoleMgr
{
public:
    RPCMgr() : _packet_handle_state(NULL)
    {
        _packet_handle_state = new CMDFSMStateRecvPkt();
    }

    virtual bool belong(const proto_header& header)
    {
        return CMD_RPC_REQ_STATE == header.cmd || CMD_RPC_KEEPALIVE_STATE == header.cmd;
    }

    virtual void* entry(CMDFSM* fsm, const proto_header& header, lcdn::base::Buffer* payload)
    {
        RPCConnection* rpc_connection = new RPCConnection();
        assert(NULL != rpc_connection);
        
        rpc_connection->set_fsm(fsm);
        rpc_connection->set_method_manager(&_method_manager);
        fsm->set_state(_packet_handle_state);

        return rpc_connection;
    }

    void register_service(gpb::Service* service);
private:
    typedef std::map<CMDFSM*, RPCConnection*> FSMToConnectionMap;

    RPCMethodManager _method_manager;
    FSMToConnectionMap _fsm_to_connection;
    CMDFSMState* _packet_handle_state;
};

} // namespace net
} // namespace lcdn

#endif // _LCDN_NET_RPC_SERVER_H_
