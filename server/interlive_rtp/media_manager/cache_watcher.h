#pragma once

#include "streamid.h"

namespace media_manager
{

    enum WatchingType
    {
        CACHE_WATCHING_CONFIG = 0x01,
        CACHE_WATCHING_FLV_HEADER = 0x02,
        CACHE_WATCHING_FLV_MINIBLOCK = 0x04,
        CACHE_WATCHING_FLV_BLOCK = 0x08,
        CACHE_WATCHING_FLV_FRAGMENT = 0x10,
        CACHE_WATCHING_TS_SEGMENT = 0x20,
        CACHE_WATCHING_SDP = 0x40,
        CACHE_WATCHING_RTP_BLOCK = 0x80,
        CACHE_WATCHING_ALL = 0xff
    };

    /**
    * @var		cache_watch_handler
    * @brief	a module using cache_manager should register a watcher in cache_manager, \n
    *			cache_manager will notify it after data set.
    * @param[in] stream_id
    * @param[in] watching_type
    * @param[in] arg
    * @return	void
    * @see		RateType \n
    *			WatchingType
    */
    typedef void(*cache_watch_handler)(StreamId_Ext, uint8_t, void*);

    class CacheWatcher
    {
    public:
        CacheWatcher(cache_watch_handler input_handler, uint8_t input_watch_type, void* input_subscriber)
        {
            handler = input_handler;
            watch_type = input_watch_type;
            subscriber = input_subscriber;
        }

    public:
        cache_watch_handler		handler;
        uint8_t					watch_type;
        void*                   subscriber;
    };

}
