#pragma once

#include "proto_define_rtp.h"
#include "utils/buffer.hpp"

int encode_rtp_u2r_req_state(const rtp_u2r_req_state *body, buffer *obuf);
int decode_rtp_u2r_req_state(rtp_u2r_req_state * body, buffer * input);
int encode_rtp_u2r_rsp_state_uint8(rtp_u2r_rsp_state* body, uint8_t* obuf, uint16_t& len);
int encode_rtp_d2p_req_state(rtp_d2p_req_state * body, buffer* obuf);
int decode_rtp_d2p_req_state(rtp_d2p_req_state * body, buffer * input);
int encode_rtp_d2p_rsp_state_uint8(rtp_d2p_rsp_state* body, uint8_t* obuf, uint16_t& len);
