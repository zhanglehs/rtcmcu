#include "rpc_common.h"

#include <arpa/inet.h>

using namespace std;
using namespace lcdn;
using namespace lcdn::base;

#define HASH_INITVAL 0x31415926

extern uint32_t hashword(
    const uint32_t *k,
    size_t          length,
    uint32_t        initval);

uint32_t hash_string(const string &str) 
{
    return static_cast<uint32_t>(hashword(
        reinterpret_cast<const uint32_t*>(str.c_str()),
        str.size() / 4, HASH_INITVAL));
}

void encode_header(Buffer* obuf, uint16_t cmd, uint32_t size)
{
    uint16_t ncmd = htons(cmd);
    uint32_t nsize = htonl(size);

    obuf->append_ptr(&MAGIC_1, sizeof(uint8_t));
    obuf->append_ptr(&VER_1, sizeof(uint8_t));
    obuf->append_ptr(&ncmd, sizeof(uint16_t));
    obuf->append_ptr(&nsize, sizeof(uint32_t));
}

// rpc
int encode_rpc_req_header(const rpc_req_header * packet, Buffer * obuf, size_t size)
{
    uint32_t total_sz = sizeof(proto_header) + sizeof(rpc_req_header) + size;

    if (obuf->capacity() < total_sz)
    {
        return -1;
    }
    
    encode_header(obuf, CMD_RPC_REQ_STATE, total_sz);
    obuf->append_ptr(packet, sizeof(rpc_req_header));
    return 0;
}

int encode_rpc_req(uint8_t* body, uint32_t len, Buffer * obuf)
{
    uint32_t total_sz = len;

    if (obuf->capacity() < total_sz)
    {
        return -1;
    }
    obuf->append_ptr(body, len);
    return 0;
}

int encode_rpc_rsp_header(const rpc_rsp_header * packet, Buffer * obuf, size_t size)
{
    uint32_t total_sz = sizeof(proto_header) + sizeof(rpc_rsp_header) + size;

    if (obuf->capacity() < total_sz)
    {
        return -1;
    }
    encode_header(obuf, CMD_RPC_RSP_STATE, total_sz);
    obuf->append_ptr(packet, sizeof(rpc_rsp_header));
    return 0;
}

int encode_rpc_rsp(uint8_t* body, uint32_t len, Buffer * obuf)
{
    uint32_t total_sz = len;

    if (obuf->capacity() < total_sz)
    {
        return -1;
    }
    obuf->append_ptr(body, len);
    return 0;
}

