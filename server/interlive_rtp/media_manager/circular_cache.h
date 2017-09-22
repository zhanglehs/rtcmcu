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

        fragment::BaseBlock* get_next_by_seq(::int32_t seq, ::int32_t& status_code, bool only_check = false);

        virtual fragment::BaseBlock* get_by_ts(uint32_t timestamp, ::int32_t& status_code, bool return_next_if_error, bool only_check = false);

        virtual fragment::BaseBlock* get_next_by_ts(uint32_t timestamp, ::int32_t& status_code, bool only_check = false);

        fragment::BaseBlock* get_latest(::int32_t& status_code, bool only_check = false);

        fragment::BaseBlock* get_latest_key(::int32_t& status_code, bool only_check = false);

        fragment::BaseBlock* get_latest_n(::int32_t& status_code, int32_t latest_n, bool only_check = false, bool is_key = false);

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
        fragment::BaseBlock* _delete(fragment::BaseBlock* block);

        virtual int32_t     _add_index(fragment::BaseBlock* block);
        virtual  int32_t     _remove_index(fragment::BaseBlock* block);

    protected:
        std::deque<fragment::BaseBlock*> _block_store; 

        time_t				_last_push_time;
        time_t				_last_req_time;
        uint32_t _temp_pushed_count;

        uint32_t _just_pop_block_timestamp;

        StreamId_Ext _stream_id;

        uint64_t _last_push_relative_timestamp_ms;
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

    /*class FLVBlockCircularCache : public CircularCache
    {
        friend class CacheManager;
    public:
        FLVBlockCircularCache(StreamId_Ext& stream_id);
        int32_t set_flv_header(flv_header* input_flv_header, uint32_t flv_header_len);
        flv_header* get_flv_header(uint32_t& header_len, int32_t& status_code, bool only_check = false);
         
        virtual ~FLVBlockCircularCache();

        virtual int32_t clear();

    protected:
        flv_header*			_flv_header;
        uint32_t				_flv_header_len;

        uint8_t				_flv_header_flag;

    };


    class P2PTimeRange
    {
    public:
        P2PTimeRange();
        int64_t set_stream_start_ts_msec(int64_t start_ts_msec);
        int64_t get_stream_start_ts_msec();
        int64_t get_stream_start_duration_msec();
        uint32_t set_buffer_ts(uint32_t start, uint32_t end);
        uint32_t get_buffer_start_ts_msec();
        uint32_t get_buffer_end_ts_msec();
        json_object* get_json();
        uint32_t get_timerange_len();
        int32_t copy_timerange_to_buffer(buffer* dst);

        ~P2PTimeRange();

    public:
        json_object* update_json();

    protected:
        int64_t _stream_start_ts_msec;
        uint32_t _buffer_start_ts_msec;
        uint32_t _buffer_end_ts_msec;

        json_object* _json;
        std::string _json_str;
    };

    class P2PonMetaData
    {
    public:
        P2PonMetaData();
        int32_t generate_p2p_header();
        int32_t set_onMetaData(const flv_header* header, uint32_t flv_header_len, uint32_t stream_start_ts_sec);
        uint32_t get_p2p_onMetaData_len();
        int32_t copy_p2p_onMetaData_to_buffer(buffer* dst);
        ~P2PonMetaData();

    protected:
        bool _has_avc0();
        bool _has_aac0();
        bool _has_script_data_object();

        fragment::flv_p2p_header _p2p_header;
        uint8_t* _onMetaData;
        uint32_t _onMetaData_len;
        uint32_t _stream_start_ts_sec;
    };


    class FLVFragmentCircularCache : public CircularCache
    {
        friend class CacheManager;
    public: 
        FLVFragmentCircularCache(StreamId_Ext& stream_id);

        virtual ::int32_t push(fragment::BaseBlock* block);
//        virtual ::int32_t pop_front(bool delete_flag = false);

        virtual fragment::BaseBlock* get_by_ts(uint32_t timestamp, ::int32_t& status_code, bool return_next_if_error, bool only_check = false);
        virtual fragment::BaseBlock* get_next_by_ts(uint32_t timestamp, ::int32_t& status_code, bool only_check = false);

        virtual ~FLVFragmentCircularCache();
        int64_t set_stream_start_ts_msec(int64_t start_ts_msec);

        P2PTimeRange* get_p2p_timerange(int32_t& status_code);
        P2PonMetaData* get_p2p_onMetaData(int32_t& status_code);

        virtual int32_t clear();

    protected:
        virtual int32_t     _add_index(fragment::BaseBlock* block);
        virtual int32_t     _remove_index(fragment::BaseBlock* block);


        P2PTimeRange* _p2p_timerange;
        P2PonMetaData* _p2p_onMetaData;

        typedef __gnu_cxx::hash_map<uint32_t, uint32_t> TimeStampMap_t;

        TimeStampMap_t _timestamp_map;
    };

    class TSSegmentCircularCache;

    class TSSegmentInfo
    {
    public:
        uint32_t seq;
        double duration;
        int64_t generate_time_ms;
        bool discontinuity;
    };

    class HLSM3U8
    {
        friend class TSSegmentCircularCache;
    public:
        HLSM3U8(StreamId_Ext& id);
        std::string& get_content_str(std::string& token);

        uint32_t size();

        ~HLSM3U8();

    protected:
       // int32_t _push_segment(uint32_t seq, double duration);
        int32_t _push_segment(TSSegmentInfo& info);
        int32_t _adjust_size(uint32_t max_size);
        int32_t _gen_header(uint32_t target_duration);

        StreamId_Ext _stream_id;
        char* _header;
        std::string _content;
      //  std::deque<std::string> _segments;
      //  std::deque<uint32_t> _segment_seqs;
        std::deque<TSSegmentInfo> _segment_infos;
    };

    class TSSegmentCircularCache : public CircularCache 
    {
        friend class CacheManager;
    public:
        TSSegmentCircularCache(StreamId_Ext& stream_id, HLSM3U8LiveCacheConfig* m3u8_config);

        int32_t	load_config(HLSM3U8LiveCacheConfig* m3u8_config);

        virtual int32_t push(fragment::BaseBlock* block);

        virtual fragment::BaseBlock* get_by_ts(uint32_t timestamp, int32_t& status_code, bool return_next_if_error, bool only_check = false);
        virtual fragment::BaseBlock* get_next_by_ts(uint32_t timestamp, int32_t& status_code, bool only_check = false);

        HLSM3U8* get_m3u8(int32_t& status_code);

        ~TSSegmentCircularCache();

        virtual int32_t clear();

    protected:
        virtual int32_t     _add_index(fragment::BaseBlock* block);
        virtual int32_t     _remove_index(fragment::BaseBlock* block);

        HLSM3U8* _generate_m3u8();

        typedef __gnu_cxx::hash_map<uint32_t, uint32_t> TimeStampMap_t;

        TimeStampMap_t _timestamp_map;

        HLSM3U8LiveCacheConfig* _m3u8_config;
        HLSM3U8* _m3u8;
    };*/
}

#endif /* __CIRCULAR_CACHE_H_ */
