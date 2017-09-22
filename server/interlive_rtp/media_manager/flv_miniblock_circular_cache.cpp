/**
* @file flv_miniblock_circular_cache.cpp
* @brief implementation of cache_manager.h
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/05/29
* @see  fragment.h \n
*		 block_circular_cache.h
*/

#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

#include <fstream>
#include <iostream>

#include "util/util.h"
#include "utils/memory.h"
#include "util/log.h"
#include "util/levent.h"
#include "util/flv.h"
#include "cache_manager.h"
#include "../backend_new/module_backend.h"
#include "circular_cache.h"

using namespace std;
using namespace fragment;

namespace media_manager
{
    FLVMiniBlockCircularCache::FLVMiniBlockCircularCache(StreamId_Ext& stream_id)
        : CircularCache(stream_id),
        _flv_header(NULL),
        _flv_header_len(0),
        _flv_header_flag(0)
    {

    }

    int32_t FLVMiniBlockCircularCache::set_flv_header(flv_header* input_flv_header, uint32_t flv_header_len)
    {
        if (flv_header_len <= 0)
        {
            return 0;
        }

        _flv_header_len = flv_header_len;

        if (_flv_header != NULL)
        {
            mfree(_flv_header);
        }

        _flv_header = (flv_header*)mmalloc(_flv_header_len);
        memcpy(_flv_header, input_flv_header, _flv_header_len);

        _flv_header_flag = _flv_header->flags & FLV_FLAG_BOTH;
        INF("set_flv_header success, stream_id: %s, length: %d", _stream_id.unparse().c_str(), _flv_header_len);

        return _flv_header_len;
    }

    flv_header* FLVMiniBlockCircularCache::get_flv_header(uint32_t& len, int32_t& status_code, bool only_check)
    {
        if (_flv_header == NULL)
        {
            status_code = STATUS_NO_DATA;

            len = 0;
            return NULL;
        }

        status_code = STATUS_SUCCESS;

        if (!only_check)
        {
            set_req_active();
        }

        len = _flv_header_len;
        return _flv_header;
    }

    int32_t FLVMiniBlockCircularCache::clear()
    {
        if (_flv_header != NULL)
        {
            mfree(_flv_header);
            _flv_header = NULL;
        }

        _flv_header_len = 0;
        _flv_header_flag = 0;

        DBG("FLVBlockCircularCache::clear(), stream_id: %s", _stream_id.unparse().c_str());

        return CircularCache::clear();
    }

    FLVMiniBlockCircularCache::~FLVMiniBlockCircularCache()
    {
        FLVMiniBlockCircularCache::clear();
        DBG("~FLVMiniBlockCircularCache(), stream_id: %s", _stream_id.unparse().c_str());
    }

}
