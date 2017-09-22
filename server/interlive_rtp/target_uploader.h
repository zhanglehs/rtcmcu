#ifndef TARGET_UPLOADER_H
#define TARGET_UPLOADER_H

#include <stdint.h>
#include "streamid.h"

struct Uploader;

struct Uploader *uploader_req_stream(uint32_t streamid);
struct Uploader *uploader_req_stream_v2(StreamId_Ext streamid);
void uploader_walk(void(*walk) (struct Uploader * u, void *), void *);


#endif
