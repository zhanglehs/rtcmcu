/*
 * author: hechao@youku.com
 * create: 2013.6.6
 *
 */

#ifndef TARGET_BACKEND_H
#define TARGET_BACKEND_H

#include "util/common.h"

#if (defined __cplusplus && !defined _WIN32)
extern "C" 
{
#endif
/*
 * return stream num that retrieved;
 * stream ids are stored in ids
 */
//int retrieve_streamid(uint32_t ids[], int array_size);

/* is streamid a src stream */
//boolean tb_is_src_stream(uint32_t streamid);
boolean tb_is_src_stream_v2(StreamId_Ext streamid);
int notify_need_stream(uint32_t streamid);
#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
