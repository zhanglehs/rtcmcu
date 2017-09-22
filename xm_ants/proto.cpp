/*
 *
 *
 */

#include "proto.h"
#include <arpa/inet.h>
#include <net/net_utils.h>
#include <assert.h>

using namespace lcdn;
using namespace lcdn::base;

const uint8_t VER = 1;
const uint8_t MAGIC = 0xff;

static inline int 
encode_header(Buffer * obuf, uint16_t cmd, uint32_t size)
{
    if (obuf->capacity() < size)
    {
        return -1;
    }
    uint16_t ncmd = htons(cmd);
    uint32_t nsize = htonl(size);

    obuf->append_ptr(&MAGIC, sizeof(uint8_t));
    obuf->append_ptr(&VER, sizeof(uint8_t));
    obuf->append_ptr(&ncmd, sizeof(uint16_t));
    obuf->append_ptr(&nsize, sizeof(uint32_t));

    return 0;
}

static int
decode_header(const Buffer * ibuf, proto_header * h)
{
    if (ibuf->data_len() < sizeof(proto_header))
        return -1; 
    const proto_header *ih = (const proto_header *) ibuf->data_ptr();

    if (VER != ih->version)
        return -1; 

    h->magic = ih->magic;
    h->version = ih->version;
    h->cmd = ntohs(ih->cmd);
    h->size = ntohl(ih->size);
    return 0;
}

int encode_u2r_req_state(const u2r_req_state *body, Buffer *obuf)
{
    // TODO 
    uint32_t total_sz = sizeof(proto_header) + sizeof(u2r_req_state);
    if (obuf->capacity() < total_sz)
    {
        return -1;
    }

    encode_header(obuf, CMD_U2R_REQ_STATE, total_sz);
    uint32_t version = htonl(body->version);
    uint32_t streamid = htonl(body->streamid);
    uint64_t user_id= net_util_htonll(body->user_id);

    obuf->append_ptr(&version, sizeof(version));
    obuf->append_ptr(&streamid, sizeof(streamid));
    obuf->append_ptr(&user_id, sizeof(user_id));
    obuf->append_ptr(&body->token, sizeof(body->token));
    obuf->append_ptr(&body->payload_type, sizeof(uint8_t));

    return 0;
}

int decode_u2r_rsp_state(u2r_rsp_state *rsp, const Buffer *ibuf)
{
    size_t actual_sz = sizeof(proto_header) + sizeof(u2r_rsp_state);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
    {
        return -1;
    }
    if (ibuf->data_len() < actual_sz || h.size < actual_sz)
    {
        return -2;
    }
    const u2r_rsp_state *body = (const u2r_rsp_state *)
        ((const char*)(ibuf->data_ptr()) + sizeof(proto_header));

    rsp->streamid = ntohl(body->streamid);
    rsp->result = ntohs(body->result);

    return 0;
}

int encode_u2r_streaming(const u2r_streaming *body, Buffer *obuf)
{
    uint32_t total_sz = sizeof(proto_header)
        + sizeof(u2r_streaming)
        + body->payload_size;

    if (obuf->capacity() < total_sz)
    {
        return -1;
    }
    encode_header(obuf, CMD_U2R_STREAMING, total_sz);
    uint32_t streamid = htonl(body->streamid);
    uint32_t payload_size = htonl(body->payload_size);

    obuf->append_ptr(&streamid, sizeof(streamid));
    obuf->append_ptr(&payload_size, sizeof(payload_size));
    obuf->append_ptr(&body->payload_type, sizeof(body->payload_type));
    obuf->append_ptr((const char *)(body->payload), body->payload_size);

    return 0;
}

int encode_u2r_req_state_v2(const u2r_req_state_v2 *body, Buffer *obuf)
{
    // TODO 
    uint32_t total_sz = sizeof(proto_header)+sizeof(u2r_req_state_v2);
    if (obuf->capacity() < total_sz)
    {
        return -1;
    }

    encode_header(obuf, CMD_U2R_REQ_STATE_V2, total_sz);
    uint32_t version = htonl(body->version);
    uint64_t user_id = net_util_htonll(body->user_id);

    obuf->append_ptr(&version, sizeof(version));
    obuf->append_ptr(&(body->streamid), sizeof(body->streamid));
    obuf->append_ptr(&user_id, sizeof(user_id));
    obuf->append_ptr(&body->token, sizeof(body->token));
    obuf->append_ptr(&body->payload_type, sizeof(uint8_t));

    return 0;
}

int decode_u2r_rsp_state_v2(u2r_rsp_state_v2 *rsp, const Buffer *ibuf)
{
    size_t actual_sz = sizeof(proto_header)+sizeof(u2r_rsp_state_v2);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
    {
        return -1;
    }
    if (ibuf->data_len() < actual_sz || h.size < actual_sz)
    {
        return -2;
    }
    const u2r_rsp_state_v2 *body = (const u2r_rsp_state_v2 *)
        ((const char*)(ibuf->data_ptr()) + sizeof(proto_header));

    memcpy(rsp->streamid, body->streamid, sizeof(rsp->streamid));
    rsp->result = ntohs(body->result);

    return 0;
}

int encode_u2r_streaming_v2(const u2r_streaming_v2 *body, Buffer *obuf)
{
    uint32_t total_sz = sizeof(proto_header)
        +sizeof(u2r_streaming_v2)
        +body->payload_size;

    if (obuf->capacity() < total_sz)
    {
        return -1;
    }
    encode_header(obuf, CMD_U2R_STREAMING_V2, total_sz);
    uint32_t payload_size = htonl(body->payload_size);

    obuf->append_ptr(&(body->streamid), sizeof(body->streamid));
    obuf->append_ptr(&payload_size, sizeof(payload_size));
    obuf->append_ptr(&body->payload_type, sizeof(body->payload_type));
    obuf->append_ptr((const char *)(body->payload), body->payload_size);

    return 0;
}

int
decode_u2r_streaming(Buffer * ibuf)
{
    assert(ibuf->data_len() >= sizeof(proto_header));

    size_t min_sz = sizeof(proto_header) + sizeof(u2r_streaming);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    
    if (ibuf->data_len() < min_sz)
        return -2;
    u2r_streaming *body = (u2r_streaming *) ((uint8_t*)(ibuf->data_ptr())
            + sizeof(proto_header));

    body->streamid = ntohl(body->streamid);
    body->payload_size = ntohl(body->payload_size);
    size_t actual_sz = min_sz + body->payload_size;

    if (h.size < actual_sz || ibuf->data_len() < actual_sz)
        return -3;
    return 0;
}
