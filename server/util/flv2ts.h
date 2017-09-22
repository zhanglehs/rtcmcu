/***************************************
 * YueHonghui, 2013-07-25
 * hhyue@tudou.com
 * ************************************/

#ifndef FLV2TS_H_
#define FLV2TS_H_
#include <stdint.h>
#include <unistd.h>
#include "utils/buffer.hpp"
#include "flv.h"

#if (defined __cplusplus && !defined _WIN32)
extern "C"
{
#endif
typedef struct
{
    uint8_t *aac_config;
    size_t aac_config_size;

    uint8_t *avc_config;
    size_t avc_config_size;

    uint8_t *flvdata;
    size_t flvdata_size;
} flv2ts_para;

int flv2ts(const flv2ts_para * paras, buffer * obuf, int64_t * duration, uint8_t flv_tag_flag = FLV_FLAG_BOTH);

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
