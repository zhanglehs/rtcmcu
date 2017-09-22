#include "delay_test.h"
#include "util/hash.h"
#include "util/log.h"
#include "util/flv.h"

void print_delay_log(StreamId_Ext streamid, flv_tag* tag, std::string module)
{
#ifdef DELAY_TEST
    if ( (tag->type == FLV_TAG_VIDEO) && (flv_get_datasize(tag->datasize) > 100) )
    {
        flv_tag_video* video = (flv_tag_video *)tag->data;
        if (video->video_info.frametype == FLV_KEYFRAME)
        {
            timeval now;
            gettimeofday(&now, NULL);
            int64_t timestamp = now.tv_sec * 1000 + now.tv_usec / 1000;
            uint32_t murmur = murmur_hash((char*)(tag->data), flv_get_datasize(tag->datasize));
            INF("delay test. %s streamid: %s hash: %08x timestamp: %ld datasize: %d",
                module.c_str(), streamid.unparse().c_str(), murmur, timestamp, flv_get_datasize(tag->datasize));
        }
    }
#endif

}
