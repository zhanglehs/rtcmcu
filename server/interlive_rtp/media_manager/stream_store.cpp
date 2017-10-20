/**
* @file stream_store.cpp
* @brief stream store of some caches
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/12/03
* @see  cache_manager.h
*/

#include "fragment/fragment.h"
#include "cache_manager.h"
#include "media_manager/circular_cache.h"
#include "fragment/fragment_generator.h"

namespace media_manager {

  StreamStore::StreamStore(const StreamId_Ext& stream_id_ext, uint8_t module_type) {
    stream_id = stream_id_ext;
    flv_miniblock_cache = new FLVMiniBlockCircularCache(stream_id);
    if (true/*MODULE_TYPE_UPLOADER == module_type*/) {
      flv_miniblock_generator = new fragment::FLVMiniBlockGenerator(stream_id);
    }
    else {
      flv_miniblock_generator = NULL;
    }
    set_push_active();
    set_req_active();
  }

  void StreamStore::set_push_active() {
    _last_push_time = time(NULL);
  }

  void StreamStore::set_req_active() {
    _last_req_time = time(NULL);
  }

  time_t& StreamStore::get_push_active_time() {
    return _last_push_time;
  }

  time_t& StreamStore::get_req_active_time() {
    return _last_req_time;
  }

  StreamStore::~StreamStore() {
    SAFE_DELETE(flv_miniblock_generator);
    SAFE_DELETE(flv_miniblock_cache);
    INF("destruct StreamStore for stream: %s", stream_id.unparse().c_str());
  }
};
