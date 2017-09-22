#ifndef PROTO_H_
#define PROTO_H_

#include <stdint.h>

#include "common/proto_define.h"
#include "utils/buffer.hpp"

const uint8_t VER_1 = 1; // v1 tracker protocol: with original forward_client/server
const uint8_t VER_2 = 2; // v2 tracker protocol: with module_tracker in forward
const uint8_t VER_3 = 3; // v3 tracker protocol: with module_tracker in forward, support long stream id

const uint8_t MAGIC_1 = 0xff;
const uint8_t MAGIC_2 = 0xf0;
const uint8_t MAGIC_3 = 0x0f;

#if (defined __cplusplus && !defined _WIN32)
extern "C" 
{
#endif

void encode_header(buffer * obuf, uint16_t cmd, uint32_t size);
void encode_header_rtp(char* obuf, size_t obuf_size, const proto_header& header);
void encode_header_uint8(uint8_t * obuf, uint32_t& len, uint16_t cmd, uint32_t size);
int decode_header(const buffer * ibuf, proto_header * h);
int decode_header_rtp(const char * ibuf, size_t ibuf_size, proto_header& h);
int decode_header_uint8(const uint8_t * ibuf, uint32_t len, proto_header * h);
int decode_f2t_header_v2(const buffer * ibuf, proto_header * header, mt2t_proto_header * tracker_header);
int decode_f2t_header_v3(const buffer * ibuf, proto_header * header, mt2t_proto_header * tracker_header);
int decode_f2t_header_v4(const buffer * ibuf, proto_header * header, mt2t_proto_header * tracker_header);
int encode_header_v2(buffer * obuf, proto_t cmd, uint32_t size);
int encode_header_v3(buffer * obuf, proto_t cmd, uint32_t size);

// set psize point to the proto_header::size in buffer.
int encode_header_v2_s(buffer * obuf, proto_t cmd, uint32_t*& psize);
int encode_header_v3_s(buffer * obuf, proto_t cmd, uint32_t*& psize);

int encode_mt2t_header(buffer * obuf, const mt2t_proto_header& tracker_header);

int encode_st2t_fpinfo_req(buffer * obuf);

//forward <----> forward v1
int encode_f2f_req_stream(const f2f_req_stream * body, buffer * obuf);
int decode_f2f_req_stream(buffer * ibuf);

int encode_f2f_unreq_stream(const f2f_unreq_stream * body, buffer * obuf);
int decode_f2f_unreq_stream(buffer * ibuf);

int encode_f2f_rsp_stream(const f2f_rsp_stream * body, buffer * obuf);
int decode_f2f_rsp_stream(buffer * ibuf);

int encode_f2f_streaming_header(const f2f_streaming_header * body, buffer * obuf);
int decode_f2f_streaming_header(buffer * ibuf);

int encode_f2f_streaming(const f2f_streaming * body, buffer * obuf);
int decode_f2f_streaming(buffer * ibuf);

//int encode_f2f_keepalive_req(const f2f_keepalive_req * body, buffer * obuf);
//int decode_f2f_keepalive_req(f2f_keepalive_req * body, const buffer * obuf);

//forward <----> forward v2

int encode_f2f_start_task(const f2f_start_task * body, buffer * obuf);
int decode_f2f_start_task(buffer * ibuf);

int encode_f2f_start_task_rsp(const f2f_start_task_rsp * body, buffer * obuf);
int decode_f2f_start_task_rsp(buffer * ibuf);

int encode_f2f_stop_task(const f2f_stop_task * body, buffer * obuf);
int decode_f2f_stop_task(buffer * ibuf);

//int encode_f2f_stop_task_rsp(const f2f_stop_task_rsp * body, buffer * obuf);
//int decode_f2f_stop_task_rsp(buffer * ibuf);

int encode_f2f_trans_data_piece(const f2f_trans_data * body, buffer * obuf);
int decode_f2f_trans_data_piece(buffer * ibuf);

//forward <----> forward v3 (support long stream id)

int encode_f2f_start_task_v3(const f2f_start_task_v3 * body, buffer * obuf);
int decode_f2f_start_task_v3(buffer * ibuf);

int encode_f2f_start_task_rsp_v3(const f2f_start_task_rsp_v3 * body, buffer * obuf);
int decode_f2f_start_task_rsp_v3(buffer * ibuf);

int encode_f2f_stop_task_v3(const f2f_stop_task_v3 * body, buffer * obuf);
int decode_f2f_stop_task_v3(buffer * ibuf);

int encode_f2f_trans_data_piece_v3(const f2f_trans_data_v3 * body, buffer * obuf);
int decode_f2f_trans_data_piece_v3(buffer * ibuf);

//uploader<---->receiver
int decode_u2r_req_state(buffer * ibuf);
int encode_u2r_rsp_state(const u2r_rsp_state * body, buffer * obuf);
int decode_u2r_streaming(buffer * ibuf);

int decode_u2r_req_state_v2(buffer * ibuf);
int encode_u2r_rsp_state_v2(const u2r_rsp_state_v2 * body, buffer * obuf);
int decode_u2r_streaming_v2(buffer * ibuf);

int encode_u2r_req_state_v2(const u2r_req_state_v2 *body, buffer * obuf);
int decode_u2r_rsp_state_v2(u2r_rsp_state_v2 *rsp, buffer *ibuf);
int encode_u2r_streaming_v2(const u2r_streaming_v2 *body, buffer * obuf);

//receiver<---->up_sche
int decode_us2r_req_up(buffer * ibuf);
int encode_r2us_rsp_up(const r2us_rsp_up * body, buffer * obuf);

//receiver<---->portal
int encode_r2p_keepalive(const r2p_keepalive * body, buffer * obuf);

//forward <----> portal
int encode_f2p_white_list_req(buffer * obuf);
int encode_f2p_keepalive(const f2p_keepalive * body, buffer * obuf);
int encode_p2f_inf_stream(const p2f_inf_stream * body, buffer * obuf);
int decode_p2f_inf_stream(buffer * ibuf);
int encode_p2f_start_stream(const p2f_start_stream * body, buffer * obuf);
int decode_p2f_start_stream(buffer * ibuf);
int encode_p2f_close_stream(const p2f_close_stream * body, buffer * obuf);
int decode_p2f_close_stream(buffer * ibuf);

int decode_p2f_inf_stream_v2(buffer * ibuf);
int decode_p2f_start_stream_v2(buffer * ibuf);

//tracker<--->forward
int encode_f2t_addr_req(const f2t_addr_req * req, buffer * obuf);
int decode_f2t_addr_req(f2t_addr_req * req, buffer * ibuf);

int encode_f2t_addr_rsp(const f2t_addr_rsp * rsp, buffer * obuf);
int decode_f2t_addr_rsp_o(buffer * ibuf);
int decode_f2t_addr_rsp(f2t_addr_rsp * rsp, buffer * ibuf);

int encode_f2t_update_stream_req(const f2t_update_stream_req * body, buffer * obuf);
int decode_f2t_update_stream_req(f2t_update_stream_req * body, buffer * ibuf);

int encode_f2t_update_rsp(const f2t_update_stream_rsp * body, buffer * obuf);
int decode_f2t_update_rsp_o(buffer * ibuf);
int decode_f2t_update_rsp(f2t_update_stream_rsp * body, buffer * ibuf);

int encode_f2t_keep_alive_req(const f2t_keep_alive_req * req, buffer * obuf);
int decode_f2t_keep_alive_req(f2t_keep_alive_req * req, buffer * ibuf);

int encode_f2t_keep_alive_rsp(const f2t_keep_alive_rsp * rsp, buffer * obuf);
int decode_f2t_keep_alive_rsp_o(buffer * ibuf);
int decode_f2t_keep_alive_rsp(f2t_keep_alive_rsp * rsp, buffer * ibuf);

int encode_f2t_register_req(const f2t_register_req_t * req, buffer * obuf);
int decode_f2t_register_req(f2t_register_req_t * req, buffer * ibuf);

int encode_f2t_register_rsp(const f2t_register_rsp_t * rsp, buffer * obuf);
int decode_f2t_register_rsp_o(buffer * ibuf);
int decode_f2t_register_rsp(f2t_register_rsp_t * rsp, buffer * ibuf);

//tracker<--->forward v2
// note: ibuf and obuf should be adjusted to the packet data before calling these functions.
// encode functions return 0 if encode ok.
int encode_f2t_addr_req_v2(const f2t_addr_req_v2 * packet, buffer * obuf);
int encode_f2t_addr_rsp_v2(const f2t_addr_rsp_v2 * packet, buffer * obuf);
int encode_f2t_update_stream_req_v2(const f2t_update_stream_req_v2 * packet, buffer * obuf);
int encode_f2t_update_rsp_v2(const f2t_update_stream_rsp_v2 * packet, buffer * obuf);
int encode_f2t_keep_alive_req_v2(const f2t_keep_alive_req_v2 * packet, buffer * obuf);
int encode_f2t_keep_alive_rsp_v2(const f2t_keep_alive_rsp_v2 * packet, buffer * obuf);
int encode_f2t_register_req_v2(const f2t_register_req_v2 * packet, buffer * obuf);
int encode_f2t_register_rsp_v2(const f2t_register_rsp_v2 * packet, buffer * obuf);
bool decode_f2t_addr_req_v2(f2t_addr_req_v2 * packet, buffer * ibuf);
bool decode_f2t_addr_rsp_v2(f2t_addr_rsp_v2 * packet, buffer * ibuf);
bool decode_f2t_update_stream_req_v2(f2t_update_stream_req_v2 * packet, buffer * ibuf);
bool decode_f2t_update_rsp_v2(f2t_update_stream_rsp_v2 * packet, buffer * ibuf);
bool decode_f2t_keep_alive_req_v2(f2t_keep_alive_req_v2 * packet, buffer * ibuf);
bool decode_f2t_keep_alive_rsp_v2(f2t_keep_alive_rsp_v2 * packet, buffer * ibuf);
bool decode_f2t_register_req_v2(f2t_register_req_v2 * packet, buffer * ibuf);
bool decode_f2t_register_rsp_v2(f2t_register_rsp_v2 * packet, buffer * ibuf);

//tracker<--->forward v3 (support long stream id)
int encode_f2t_addr_req_v3(const f2t_addr_req_v3 * packet, buffer * obuf);
int encode_f2t_addr_rsp_v3(const f2t_addr_rsp_v3 * packet, buffer * obuf);
int encode_f2t_update_stream_req_v3(const f2t_update_stream_req_v3 * packet, buffer * obuf);
int encode_f2t_update_stream_rsp_v3(const f2t_update_stream_rsp_v3 * packet, buffer * obuf);
int encode_f2t_keep_alive_req_v3(const f2t_keep_alive_req_v3 * packet, buffer * obuf);
int encode_f2t_keep_alive_rsp_v3(const f2t_keep_alive_rsp_v3 * packet, buffer * obuf);
int encode_f2t_server_stat_rsp(const f2t_server_stat_rsp * packet, buffer * obuf);
int encode_f2t_register_req_v3(const f2t_register_req_v3 * packet, buffer * obuf);
int encode_f2t_register_rsp_v3(const f2t_register_rsp_v3 * packet, buffer * obuf);
bool decode_f2t_addr_req_v3(f2t_addr_req_v3 * packet, buffer * ibuf);
bool decode_f2t_addr_rsp_v3(f2t_addr_rsp_v3 * packet, buffer * ibuf);
bool decode_f2t_update_stream_req_v3(f2t_update_stream_req_v3 * packet, buffer * ibuf);
bool decode_f2t_update_stream_rsp_v3(f2t_update_stream_rsp_v3 * packet, buffer * ibuf);
bool decode_f2t_keep_alive_req_v3(f2t_keep_alive_req_v3 * packet, buffer * ibuf);
bool decode_f2t_keep_alive_rsp_v3(f2t_keep_alive_rsp_v3 * packet, buffer * ibuf);
bool decode_f2t_server_stat_rsp(f2t_server_stat_rsp * packet, buffer * ibuf);
bool decode_f2t_register_req_v3(f2t_register_req_v3 * packet, buffer * ibuf);
bool decode_f2t_register_rsp_v3(f2t_register_rsp_v3 * packet, buffer * ibuf);


int decode_t2st_fpinfo_rsp(buffer *ibuf);
#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
