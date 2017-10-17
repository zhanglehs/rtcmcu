#pragma once

#include "proto_define_rtp.h"
#include "utils/buffer.hpp"
#include "protobuf/proto_rtp_ext.pb.h"


int decode_rtp_proto(uint8_t* output, uint8_t* input, RTPProtoType& type, uint16_t& len);

int decode_rtp_streamid_proto(uint8_t* output, uint8_t* input, uint8_t* stream_id, RTPProtoType& type, uint16_t& len);

int decode_rtp_cmd_proto(uint8_t* output, uint8_t* input, RTPProtoType& type, uint16_t& len);


int encode_rtp_proto_uint8(uint8_t* buffer, uint16_t& len, RTPProtoType type, const uint8_t* payload, uint16_t data_len);

int encode_rtp_streamid_proto_uint8(uint8_t* buffer, uint16_t& len, RTPProtoType type, uint8_t* stream_id, const uint8_t* payload, uint16_t data_len);

int encode_rtp_cmd_proto_uint8(uint8_t* buffer, uint16_t& len, const uint8_t* payload, uint16_t data_len);


int encode_rtp_proto(buffer* buf, RTPProtoType type, const uint8_t* payload, uint16_t data_len);

int encode_rtp_streamid_proto(buffer* buf, RTPProtoType type, uint8_t* stream_id, const uint8_t* payload, uint16_t data_len);

int encode_rtp_cmd_proto(buffer* buf, const uint8_t* payload, uint16_t data_len);

int decode_rtp_f2f_req_state(uint8_t* input, uint16_t len, rtp_f2f_req_state* body);
int encode_rtp_f2f_req_state(rtp_f2f_req_state* body, buffer* obuf);

int encode_rtp_f2f_req_state_uint8(rtp_f2f_req_state* body, uint8_t* obuf, uint16_t& len);

// rtp uploader <----> rtp receiver
int encode_rtp_u2r_req_state(const rtp_u2r_req_state *body, buffer *obuf);
int decode_rtp_u2r_req_state(uint8_t* input, uint16_t len, rtp_u2r_req_state* body,live_stream_sdk::RtpU2rExtension *extension);
int decode_rtp_u2r_req_state(rtp_u2r_req_state * body,live_stream_sdk::RtpU2rExtension *extension, buffer * input);
int encode_rtp_u2r_rsp_state(const rtp_u2r_rsp_state* body, buffer* obuf);
int encode_rtp_u2r_rsp_state_uint8(rtp_u2r_rsp_state* body, uint8_t* obuf, uint16_t& len);
int encode_rtp_u2r_packet(uint8_t* body, uint32_t len, buffer * obuf);
int encode_rtp_u2r_packet_header(rtp_u2r_packet_header* body, buffer * obuf, size_t size);
int encode_rtcp_u2r_packet(uint8_t* body, uint32_t len, buffer * obuf);
int encode_rtcp_u2r_packet_header(rtcp_u2r_packet_header* body, buffer * obuf, size_t size);

// rtp downloader <----> rtp player
int decode_rtp_d2p_req_state(uint8_t* input, uint16_t len, rtp_d2p_req_state* body,live_stream_sdk::RtpD2pExtension *extension);
int encode_rtp_d2p_req_state(rtp_d2p_req_state * body,buffer* obuf);
int decode_rtp_d2p_req_state(rtp_d2p_req_state * body, live_stream_sdk::RtpD2pExtension *extension,buffer * input);
int encode_rtp_d2p_rsp_state(rtp_d2p_rsp_state* body, buffer* obuf);
int encode_rtp_d2p_rsp_state_uint8(rtp_d2p_rsp_state* body, uint8_t* obuf, uint16_t& len);
int encode_rtp_d2p_packet(uint8_t* body, uint32_t len, buffer * obuf);
int encode_rtp_d2p_packet_header(rtp_d2p_packet_header* body, buffer * obuf, size_t size);
int encode_rtcp_d2p_packet(uint8_t* body, uint32_t len, buffer * obuf);
int encode_rtcp_d2p_packet_header(rtcp_d2p_packet_header* body, buffer * obuf, size_t size);
int decode_rtcp_d2p_packet(rtcp_d2p_packet_header * body, buffer * input);
int decode_rtp_d2p_packet(rtp_d2p_packet_header * body, buffer * input);

// rtp f2f
int encode_f2f_rtp_packet_header(const f2f_rtp_packet_header * body, char* buf, size_t size);
int decode_f2f_rtp_packet_header(f2f_rtp_packet_header * body, const char* buf, size_t size);
