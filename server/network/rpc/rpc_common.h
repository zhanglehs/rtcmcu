#ifndef _LCDN_NET_RPC_COMMON_H_
#define _LCDN_NET_RPC_COMMON_H_

#include "../../common/proto_define.h"
#include "utils/buffer.hpp"
#include <string>


const uint8_t VER_1 = 1; // v1 tracker protocol: with original forward_client/server
const uint8_t VER_2 = 2; // v2 tracker protocol: with module_tracker in forward
const uint8_t VER_3 = 3; // v3 tracker protocol: with module_tracker in forward, support long stream id

const uint8_t MAGIC_1 = 0xff;
const uint8_t MAGIC_2 = 0xf0;
const uint8_t MAGIC_3 = 0x0f;

const int MESSAGE_MAX_LENGTH = 512 * 1024;

uint32_t hash_string(const std::string &str);

#pragma pack(1)

// rpc
typedef struct rpc_req_header
{
    uint32_t seq;
    uint32_t opcode;
} rpc_req_header;

typedef struct rpc_rsp_header
{
    uint32_t seq;
    uint32_t opcode;
} rpc_rsp_header;

#pragma pack()

void encode_header(lcdn::base::Buffer *obuf, uint16_t cmd, uint32_t size);

// rpc
int encode_rpc_req_header(const rpc_req_header* packet, lcdn::base::Buffer * obuf, size_t size);
int encode_rpc_req(uint8_t* body, uint32_t len, lcdn::base::Buffer * obuf);
int encode_rpc_rsp_header(const rpc_rsp_header* packet, lcdn::base::Buffer * obuf, size_t size);
int encode_rpc_rsp(uint8_t* body, uint32_t len, lcdn::base::Buffer * obuf);

#endif // _LCDN_NET_RPC_COMMON_H_
