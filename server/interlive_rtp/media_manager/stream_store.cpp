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

#include "fragment.h"
#include "cache_manager.h"

using namespace fragment;

namespace media_manager
{
    StreamStore::StreamStore(const StreamId_Ext& stream_id_ext)
        :_last_push_relative_timestamp_ms(0)
    {
        stream_id = stream_id_ext;
        //tracker_info = NULL;
        create_time = time(NULL);

        flv_miniblock_generator = NULL;
        flv_miniblock_cache = NULL;

        rtp_media_cache = NULL;

        state = STREAM_STORE_CONSTRUCT;

        this->set_push_active();
        this->set_req_active();
    }

    int32_t StreamStore::init(uint8_t module_type, CacheManagerConfig* cache_manager_config, CacheManager* cache_manager)
    {
        if (state != STREAM_STORE_CONSTRUCT)
        {
            ERR("StreamStore state is wrong, state = %d", state);
            return -1;
        }

        switch (module_type)
        {
        case MODULE_TYPE_UPLOADER:
            flv_miniblock_generator = new FLVMiniBlockGenerator(stream_id);
        case MODULE_TYPE_BACKEND:
            flv_miniblock_cache = new FLVMiniBlockCircularCache(stream_id);
            rtp_media_cache = new RTPMediaCache(stream_id);
            rtp_media_cache->set_manager(cache_manager);
            break;

        default:
            break;
        }

        set_push_active();
        set_req_active();

        state = STREAM_STORE_INIT;
        return state;
    }

    time_t& StreamStore::set_push_active()
    {
        _last_push_time = time(NULL);
        return _last_push_time;
    }

    time_t& StreamStore::set_req_active()
    {
        _last_req_time = time(NULL);
        return _last_req_time;
    }

    time_t& StreamStore::get_push_active_time()
    {
        return _last_push_time;
    }

    time_t& StreamStore::get_req_active_time()
    {
        return _last_req_time;
    }

    uint32_t StreamStore::get_last_block_seq()
    {
        uint32_t seq = 0;

        if (rtp_media_cache != NULL)
        {
            _last_push_relative_timestamp_ms = MAX(_last_push_relative_timestamp_ms, rtp_media_cache->get_last_push_relative_timestamp_ms());
            seq = MAX(seq, _last_push_relative_timestamp_ms / 100); // flv block duartion is 100 ms.
        }

        return seq;
    }

    uint64_t StreamStore::get_last_push_relative_timestamp_ms()
    {

        if (rtp_media_cache != NULL)
        {
            _last_push_relative_timestamp_ms = MAX(_last_push_relative_timestamp_ms, rtp_media_cache->get_last_push_relative_timestamp_ms());
        }

        return _last_push_relative_timestamp_ms;
    }


    StreamStore::~StreamStore()
    {
        SAFE_DELETE(flv_miniblock_generator);
        SAFE_DELETE(flv_miniblock_cache);
        
        SAFE_DELETE(rtp_media_cache);

        INF("destruct StreamStore for stream: %s", stream_id.unparse().c_str());
    }
};
