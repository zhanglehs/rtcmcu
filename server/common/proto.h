#ifndef PROTO_H_
#define PROTO_H_

#include "common/proto_define.h"
#include "utils/buffer.hpp"
#include <stdint.h>

#if (defined __cplusplus && !defined _WIN32)
extern "C" 
{
#endif

void encode_header(buffer * obuf, uint16_t cmd, uint32_t size);
void encode_header_uint8(uint8_t * obuf, uint32_t& len, uint16_t cmd, uint32_t size);
int decode_header(const buffer * ibuf, proto_header * h);

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
