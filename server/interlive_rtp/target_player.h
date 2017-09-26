#ifndef TARGET_PLAYER_H_
#define TARGET_PLAYER_H_
#include <stdint.h>
#include <string>
#include "avformat/sdp.h"
#include "streamid.h"

struct player_stream;
//struct player_stream *player_req_stream(uint32_t streamid);
struct player_stream *player_get_stream(uint32_t streamid);
void player_walk(void (*walk) (struct player_stream *, void *), void *data);
//void get_sdp_cb(StreamId_Ext sid, std::string &sdp);

#endif
