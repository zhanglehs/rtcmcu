#define _BSD_SOURCE
#include "proto.h"
#include "../util/util.h"
#include <endian.h>
#include <arpa/inet.h>
#include <assert.h>

const uint8_t VER_1 = 1; // v1 tracker protocol: with original forward_client/server
const uint8_t VER_2 = 2; // v2 tracker protocol: with module_tracker in forward
const uint8_t VER_3 = 3; // v3 tracker protocol: with module_tracker in forward, support long stream id
const uint8_t MAGIC_1 = 0xff;
const uint8_t MAGIC_2 = 0xf0;
const uint8_t MAGIC_3 = 0x0f;

void
encode_header(buffer * obuf, uint16_t cmd, uint32_t size)  //=
{
    uint16_t ncmd = htons(cmd);
    uint32_t nsize = htonl(size);

    buffer_append_ptr(obuf, &MAGIC_1, sizeof(uint8_t));
    buffer_append_ptr(obuf, &VER_1, sizeof(uint8_t));
    buffer_append_ptr(obuf, &ncmd, sizeof(uint16_t));
    buffer_append_ptr(obuf, &nsize, sizeof(uint32_t));
}

void encode_header_uint8(uint8_t* obuf, uint32_t& len, uint16_t cmd, uint32_t size) //=
{
    uint16_t ncmd = htons(cmd);
    uint32_t nsize = htonl(size);

    obuf[0] = MAGIC_1;
    obuf[1] = VER_1;
    memcpy(&obuf[2], &ncmd, 2);
    memcpy(&obuf[4], &nsize, 4);

    len = sizeof(proto_header);
}

int decode_header(const buffer * ibuf, proto_header * h) //=
{
    if (buffer_data_len(ibuf) < sizeof(proto_header))
        return -1;
    proto_header *ih = (proto_header *)buffer_data_ptr(ibuf);

    switch (ih->version)
    {
    case VER_1:
        break;

    case VER_2:
        if (ih->magic != MAGIC_2)
            return -1;
        break;

    case VER_3:
        if (ih->magic != MAGIC_3)
            return -1;
        break;

    default:
        return -1;
    }

    if (h != NULL)
    {
        h->magic = ih->magic;
        h->version = ih->version;
        h->cmd = ntohs(ih->cmd);
        h->size = ntohl(ih->size);
    }
    else
    {
        return -1;
    }

	return 0;
}

int encode_rtp_d2p_req_state(rtp_d2p_req_state * body, buffer* obuf) {
  if (body == NULL || obuf == NULL)
  {
    return -1;
  }

  uint32_t total_sz = sizeof(proto_header)+sizeof(rtp_d2p_req_state);
  encode_header(obuf, CMD_RTP_D2P_REQ_STATE, total_sz);

  uint32_t version = htonl(body->version);

  obuf->append_ptr(&version, sizeof(version));
  obuf->append_ptr(&(body->streamid), sizeof(body->streamid));
  obuf->append_ptr(&body->token, sizeof(body->token));
  obuf->append_ptr(&body->payload_type, sizeof(uint8_t));
  obuf->append_ptr(&body->useragent, sizeof(body->useragent));
  return 0;
}

int encode_rtp_u2r_req_state(const rtp_u2r_req_state *body, buffer *obuf)
{
  if (body == NULL || obuf == NULL)
  {
    return -1;
  }

  uint32_t total_sz = sizeof(proto_header)+sizeof(rtp_u2r_req_state);
  encode_header(obuf, CMD_RTP_U2R_REQ_STATE, total_sz);

  uint32_t version = htonl(body->version);
  uint64_t user_id = util_ntohll(body->user_id);

  obuf->append_ptr(&version, sizeof(version));
  obuf->append_ptr(&(body->streamid), sizeof(body->streamid));
  obuf->append_ptr(&user_id, sizeof(user_id));
  obuf->append_ptr(&body->token, sizeof(body->token));
  obuf->append_ptr(&body->payload_type, sizeof(uint8_t));

  return 0;
}

int encode_rtp_u2r_rsp_state_uint8(rtp_u2r_rsp_state* body, uint8_t* obuf, uint16_t& len)
{
  uint32_t total_sz = sizeof(proto_header)+sizeof(rtp_u2r_rsp_state);
  uint32_t offset;
  encode_header_uint8(obuf, offset, CMD_RTP_U2R_RSP_STATE, total_sz);

  uint32_t nversion = htonl(body->version);
  uint16_t nresult = htons(body->result);
  memcpy(obuf + offset, &nversion, sizeof(body->version));
  offset += sizeof(body->version);
  memcpy(obuf + offset, body->streamid, sizeof(body->streamid));
  offset += sizeof(body->streamid);
  memcpy(obuf + offset, &nresult, sizeof(body->result));
  offset += sizeof(body->result);

  len = offset;

  return 0;
}

int encode_rtp_d2p_rsp_state_uint8(rtp_d2p_rsp_state* body, uint8_t* obuf, uint16_t& len)
{
  uint32_t total_sz = sizeof(proto_header)+sizeof(rtp_u2r_rsp_state);
  uint32_t offset;
  encode_header_uint8(obuf, offset, CMD_RTP_D2P_RSP_STATE, total_sz);

  uint32_t nversion = htonl(body->version);
  uint16_t nresult = htons(body->result);
  memcpy(obuf + offset, &nversion, sizeof(body->version));
  offset += sizeof(body->version);
  memcpy(obuf + offset, body->streamid, sizeof(body->streamid));
  offset += sizeof(body->streamid);
  memcpy(obuf + offset, &nresult, sizeof(body->result));
  offset += sizeof(body->result);

  len = offset;

  return 0;
}

int
decode_rtp_u2r_req_state(rtp_u2r_req_state * body/*,live_stream_sdk::RtpU2rExtension *extension*/, buffer * ibuf)
{
  assert(buffer_data_len(ibuf) >= sizeof(proto_header));
  size_t actual_sz = sizeof(proto_header)+sizeof(rtp_u2r_req_state);
  proto_header h;

  if (0 != decode_header(ibuf, &h))
    return -1;
  if (buffer_data_len(ibuf) < actual_sz || h.size < actual_sz)
    return -2;
  rtp_u2r_req_state * data = (rtp_u2r_req_state *)(buffer_data_ptr(ibuf)
    + sizeof(proto_header));
  memmove(body, data, sizeof(rtp_u2r_req_state));

  body->version = ntohl(body->version);
  body->user_id = util_ntohll(body->user_id);

  return 0;
}

int
decode_rtp_d2p_req_state(rtp_d2p_req_state * body/*,live_stream_sdk::RtpD2pExtension *extension*/, buffer * ibuf)
{
  assert(buffer_data_len(ibuf) >= sizeof(proto_header)+sizeof(rtp_d2p_req_state));
  size_t min_sz = sizeof(proto_header)+sizeof(rtp_d2p_req_state);
  proto_header h;

  if (0 != decode_header(ibuf, &h))
    return -1;
  if (buffer_data_len(ibuf) < min_sz || h.size < min_sz)
    return -2;
  rtp_d2p_req_state* data = (rtp_d2p_req_state*)(buffer_data_ptr(ibuf) + sizeof(proto_header));
  memmove(body, data, sizeof(rtp_d2p_req_state));
  body->version = ntohl(data->version);

  return 0;
}
