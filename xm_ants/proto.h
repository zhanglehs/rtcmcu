/*
 * author: hechao@youku.com
 * create: 2014/3/13
 *
 */

#ifndef __proto_h_
#define __proto_h_

#include <utils/buffer.hpp>
#include "proto_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

// @return:
//      -1 error
//      0 succ
int encode_u2r_req_state(const u2r_req_state *body, lcdn::base::Buffer *obuf);

// @return:
//      -1 error
//      0  succ
//      N  need more bytes
int decode_u2r_rsp_state(u2r_rsp_state *rsp, const lcdn::base::Buffer *ibuf);

// @return:
//      -1 error
//      0 succ
int encode_u2r_streaming(const u2r_streaming *body, lcdn::base::Buffer *obuf);

// @return:
//      -1 error
//      0 succ
int encode_u2r_req_state_v2(const u2r_req_state_v2 *body, lcdn::base::Buffer *obuf);

// @return:
//      -1 error
//      0  succ
//      N  need more bytes
int decode_u2r_rsp_state_v2(u2r_rsp_state_v2 *rsp, const lcdn::base::Buffer *ibuf);

// @return:
//      -1 error
//      0 succ
int encode_u2r_streaming_v2(const u2r_streaming_v2 *body, lcdn::base::Buffer *obuf);

int decode_u2r_streaming(lcdn::base::Buffer * ibuf);

#ifdef __cplusplus
}
#endif

#endif

