/**
* @file
* @brief
* @author   songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2015/07/24
* @see
*/

#pragma once

#include <stdint.h>
#include "rtp_block_cache.h"
#include "avformat/sdp.h"
#include "cache_watcher.h"

#include "streamid.h"

namespace media_manager {

  class MediaManagerRTPInterface {
  public:
    virtual RTPMediaCache* get_rtp_media_cache(const StreamId_Ext& stream_id, int32_t& status_code, bool req_from_backend = true) = 0;
    virtual int32_t init_stream(const StreamId_Ext& stream_id) = 0;
    virtual void on_timer() = 0;
    virtual int32_t notify_watcher(StreamId_Ext& stream_id, uint8_t watch_type) = 0;
    virtual int32_t register_watcher(cache_watch_handler handler, uint8_t watch_type = CACHE_WATCHING_ALL, void* arg = NULL) = 0;
    virtual ~MediaManagerRTPInterface(){};
  };

}
