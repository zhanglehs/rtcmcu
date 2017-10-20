#ifndef PROTO_H_
#define PROTO_H_

#include "cmd_protocol/proto_define.h"
#include "utils/buffer.hpp"
#include <stdint.h>

#if (defined __cplusplus && !defined _WIN32)
extern "C" 
{
#endif

void encode_header(buffer * obuf, uint16_t cmd, uint32_t size);
void encode_header_uint8(uint8_t * obuf, uint32_t& len, uint16_t cmd, uint32_t size);
int decode_header(const buffer * ibuf, proto_header * h);
int encode_rtp_u2r_req_state(const rtp_u2r_req_state *body, buffer *obuf);
int decode_rtp_u2r_req_state(rtp_u2r_req_state * body, buffer * input);
int encode_rtp_u2r_rsp_state_uint8(rtp_u2r_rsp_state* body, uint8_t* obuf, uint16_t& len);
int encode_rtp_d2p_req_state(rtp_d2p_req_state * body, buffer* obuf);
int decode_rtp_d2p_req_state(rtp_d2p_req_state * body, buffer * input);
int encode_rtp_d2p_rsp_state_uint8(rtp_d2p_rsp_state* body, uint8_t* obuf, uint16_t& len);

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
