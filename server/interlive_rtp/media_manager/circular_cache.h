/**
* @file circular_cache.h
* @brief a cache using deque
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/11/20
*/

#ifndef __CIRCULAR_CACHE_H_
#define __CIRCULAR_CACHE_H_

#include <time.h>
#include <deque>
#include <string>
#include <map>
//#include <ext/hash_map>

#include "util/log.h"

#include "streamid.h"
#include "fragment/fragment.h"
#include "cache_manager_config.h"
#include <json.h>

#ifdef _WIN32
#define NULL 0
#endif

namespace media_manager
{

  class CircularCache
  {
    friend class CacheManager;
  public:
    CircularCache(StreamId_Ext& stream_id);

    virtual ::int32_t push(fragment::BaseBlock* block);

    fragment::BaseBlock* get_by_seq(uint32_t seq, ::int32_t& status_code, bool return_next_if_error, bool only_check = false);

    virtual fragment::BaseBlock* get_by_ts(uint32_t timestamp, ::int32_t& status_code, bool return_next_if_error, bool only_check = false);

    fragment::BaseBlock* get_latest(::int32_t& status_code, bool only_check = false);

    fragment::BaseBlock* get_latest_key(::int32_t& status_code, bool only_check = false);

    fragment::BaseBlock* front(::int32_t& status_code, bool only_check = false);

    fragment::BaseBlock* back(::int32_t& status_code, bool only_check = false);

    uint32_t size();

    StreamId_Ext& stream_id();

    virtual ::int32_t pop_front(bool delete_flag = false);

    virtual ~CircularCache();

    time_t reset_push_active();
    time_t reset_req_active();

    time_t set_push_active();
    time_t set_req_active();

    time_t get_push_active_time();
    time_t get_req_active_time();

    uint64_t get_last_push_relative_timestamp_ms();

    virtual int32_t clear();

  protected:
    std::deque<fragment::BaseBlock*> _block_store;

    time_t				_last_push_time;
    time_t				_last_req_time;
    uint32_t _temp_pushed_count;

    uint32_t _just_pop_block_timestamp;

    StreamId_Ext _stream_id;

    //uint64_t _last_push_relative_timestamp_ms;
  };

  class FLVMiniBlockCircularCache : public CircularCache
  {
    friend class CacheManager;
  public:
    FLVMiniBlockCircularCache(StreamId_Ext& stream_id);
    int32_t set_flv_header(flv_header* input_flv_header, uint32_t flv_header_len);
    flv_header* get_flv_header(uint32_t& header_len, int32_t& status_code, bool only_check = false);

    virtual ~FLVMiniBlockCircularCache();

    virtual int32_t clear();

  protected:
    flv_header*			_flv_header;
    uint32_t				_flv_header_len;

    uint8_t				_flv_header_flag;

  };

}

#endif /* __CIRCULAR_CACHE_H_ */
