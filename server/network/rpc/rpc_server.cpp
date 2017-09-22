#include "rpc_server.h"

using namespace lcdn;
using namespace lcdn::base;
using namespace lcdn::net;

void serialize_from_message(const gpb::Message* msg, Buffer* buf) 
{
    char temp[MESSAGE_MAX_LENGTH];
    msg->SerializeToArray(temp, MESSAGE_MAX_LENGTH);
    buf->append_ptr(temp, msg->ByteSize());
}

bool deserialize_to_message(gpb::Message* msg, const char* buf, size_t payload_len)
{
    bool result = msg->ParseFromArray(buf, static_cast<int>(payload_len));

    if (!result)
    {
        // TODO log
    }

    return result;
}

void RPCMethodManager::register_service(gpb::Service* service)
{
    assert(NULL != service);

    const gpb::ServiceDescriptor *descriptor = service->GetDescriptor();
    for (int i = 0; i < descriptor->method_count(); ++i)
    {
        const gpb::MethodDescriptor *method = descriptor->method(i);
        const google::protobuf::Message *request = &service->GetRequestPrototype(method);
        const google::protobuf::Message *response = &service->GetResponsePrototype(method);

        RPCMethod* rpc_method = new RPCMethod(service, request, response, method);
        uint32_t opcode = hash_string(method->full_name());
        _rpc_method_map[opcode] = rpc_method;
    }
}

static void handle_service_done(HandleServiceEntry* entry)
{
    rpc_rsp_header rpc_header;
    rpc_header.opcode = entry->_opcode;
    rpc_header.seq = entry->_seq;
    uint32_t length = entry->_response->ByteSize();

    Buffer buf(MESSAGE_MAX_LENGTH + 16);
    encode_rpc_rsp_header(&rpc_header, &buf, length);
    serialize_from_message(entry->_response, &buf);
    entry->_fsm->send(&buf);

    delete entry->_request;
    delete entry->_response;
    delete entry;
}

bool RPCMethodManager::handle_packet(const rpc_req_header& header,
                                     const char* payload,
                                     size_t payload_len,
                                     CMDFSM* fsm)
{
    uint32_t opcode = header.opcode;
    RPCMethod *rpc_method = _rpc_method_map[opcode];
    if (rpc_method == NULL) 
    {
        return false;
    }
    const gpb::MethodDescriptor *method = rpc_method->_method;
    gpb::Message *request = rpc_method->_request->New();
    gpb::Message *response = rpc_method->_response->New();
    if (!::deserialize_to_message(request, payload, payload_len))
    {
        delete request;
        delete response;
    }
    HandleServiceEntry *entry = new HandleServiceEntry(opcode,
                                                       header.seq,
                                                       request, 
                                                       response, 
                                                       fsm);
    gpb::Closure *done = gpb::NewCallback(&handle_service_done,
                                          entry);
    rpc_method->_service->CallMethod(method,
                                     NULL,
                                     request,
                                     response,
                                     done);
    return true;
}

void RPCConnection::handle_packet(Buffer* payload, size_t payload_len)
{
    rpc_req_header rpc_header;
    decode_rpc_header(payload, rpc_header);

    const char* rpc_payload_offset = static_cast<const char*>(
        payload->data_ptr()) + sizeof(rpc_req_header);
    size_t rpc_payload_len = payload_len - sizeof(rpc_req_header);

    _method_manager->handle_packet(rpc_header, rpc_payload_offset, rpc_payload_len, _fsm);
}

void RPCConnection::decode_rpc_header(Buffer* pay_load, rpc_req_header& rpc_header)
{
    rpc_req_header* header = (rpc_req_header*)(pay_load->data_ptr());
    assert(NULL != header);

    rpc_header.opcode = header->opcode;
    rpc_header.seq = header->seq;
}

void RPCMgr::register_service(gpb::Service* service)
{
    _method_manager.register_service(service);
}
