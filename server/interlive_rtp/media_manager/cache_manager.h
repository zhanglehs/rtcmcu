/**
* @file cache_manager.h
* @brief	This file define some classes for stream cache manager. \n
*			The most important class is class CacheManager, \n
*			it has two derive classes, UploaderCacheManager and BackendCacheManager, \n
*			using for module uploader and module backend. \n
*			CacheManager is an singleton in one process, it will manage the cache for many streams.  \n
*			For every stream, it has two cache stores, \n
*			LatestFragmentCache and HistoryFragmentCache, \n
*			using for cache live stream and timeshift stream. \n
*			Stream data was stored in cache as FLVFragment, about 10 second data, \n
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/05/29
* @see  fragment.h
*/

#pragma once

#include "cache_watcher.h"
#include <event.h>
#include <map>
#include <vector>

namespace fragment {
  class FragmentGenerator;
  class FLVMiniBlockGenerator;
  class FLVHeader;
  class FLVMiniBlock;
}

//namespace http {
//  class HTTPServer;
//}

struct flv_header;
struct flv_tag;
struct json_object;

namespace media_manager {

  enum CacheReqStatus {
    CACHE_REQ_STREAM_CONFIG = 1,
    CACHE_REQ_STOP = 9,

    CACHE_REQ_LIVE_FLV_HEADER = 100,
    CACHE_REQ_SHIFT_FLV_HEADER = 110,

    CACHE_REQ_LIVE_FLV_LATEST_KEY_BLOCK = 200,
    CACHE_REQ_LIVE_FLV_BLOCK = 201,
    CACHE_REQ_SHIFT_FLV_BLOCK = 210,

    CACHE_REQ_LIVE_FLV_LATEST_KEY_FRAGMENT = 300,
    CACHE_REQ_LIVE_FLV_LATEST_N_FRAGMENT = 301,
    CACHE_REQ_LIVE_FLV_LATEST_FRAGMENT = 302,

    CACHE_REQ_LIVE_FLV_FRAGMENT_BY_SEQ = 310,
    CACHE_REQ_LIVE_FLV_FRAGMENT_BY_TS = 311,
    // CACHE_REQ_SHIFT_FLV_FRAGMENT = 320,

    CACHE_REQ_LIVE_TS_LATEST_SEGMENT = 400,
    CACHE_REQ_LIVE_TS_LATEST_N_SEGMENT = 401,

    CACHE_REQ_LIVE_TS_SEGMENT_BY_SEQ = 420,

    CACHE_REQ_LIVE_SDP = 500,
    CACHE_REQ_LIVE_RTP = 510,

    CACHE_REQ_LIVE_FLV_LATEST_MINIBLOCK = 600,
    CACHE_REQ_LIVE_FLV_MINIBLOCK = 601,
    CACHE_REQ_LIVE_FLV_MINIBLOCK_HEADER = 610,
  };

  enum ModuleType {
    MODULE_TYPE_UPLOADER = 1,
    MODULE_TYPE_BACKEND = 2
  };

  class FLVMiniBlockCircularCache;

  class StreamStore {
  public:
    StreamStore(const StreamId_Ext& stream_id_ext, uint8_t module_type);
    ~StreamStore();

    void set_push_active();
    void set_req_active();
    time_t& get_push_active_time();
    time_t& get_req_active_time();

  public:
    StreamId_Ext stream_id;
    fragment::FLVMiniBlockGenerator* flv_miniblock_generator;
    FLVMiniBlockCircularCache* flv_miniblock_cache;

  protected:
    time_t				_last_push_time;
    time_t				_last_req_time;
  };

  class FlvCacheManagerStatistic;
  class CacheManagerConfig;

  /**
  * @class	CacheManager
  * @brief	This is base cache manager class, \n
  *			you can use this class to get header/block/fragment. \n
  *			If you want to set value, you can use the derived classes.
  * @see		FLVBlock \n
  *			FLVFragment \n
  *			UploaderCacheManagerInterface \n
  *			BackendCacheManagerInterface \n
  *			PlayerCacheManagerInterface
  */
  class FlvCacheManager {
    friend FlvCacheManagerStatistic;

  public:
    static FlvCacheManager* Instance();
    static void DestroyInstance();

    void Init(event_base* base);
    //void set_http_server(http::HTTPServer *server);
    int32_t load_config(const CacheManagerConfig* config);

    int32_t register_watcher(cache_watch_handler handler, uint8_t watch_type, void* arg);
    int32_t destroy_stream();
    int32_t destroy_stream(StreamId_Ext stream_id);

    int32_t set_flv_header(StreamId_Ext stream_id, flv_header* input_flv_header, uint32_t flv_header_len);
    int32_t set_flv_tag(StreamId_Ext stream_id, flv_tag* input_flv_tag);
    int get_miniblock_flv_header(StreamId_Ext stream_id, fragment::FLVHeader &header);
    fragment::FLVMiniBlock* get_latest_miniblock(StreamId_Ext stream_id);
    fragment::FLVMiniBlock* get_miniblock_by_seq(StreamId_Ext stream_id, int32_t seq);

  protected:
    FlvCacheManager();
    ~FlvCacheManager();

    void StartTimer();
    static void OnTimer(const int32_t fd, short which, void *arg);
    void OnTimerImpl();

    bool contains_stream(const StreamId_Ext& stream_id);
    int32_t _notify_watcher(StreamId_Ext& stream_id, uint8_t watch_type = CACHE_WATCHING_ALL);
    int32_t _req_from_backend(StreamId_Ext stream_id, int32_t request_state, int32_t seq = 0);
    int32_t _req_stop_from_backend(StreamId_Ext stream_id, int32_t request_state, int32_t seq = 0);
    int32_t _req_stop_from_backend(uint32_t stream_id);
    int32_t _req_stop_from_backend_rtp(StreamId_Ext stream_id);
    int32_t _req_stop_from_backend(StreamId_Ext stream_id);

    void _check_stream_store_timeout();
    void _adjust_flv_miniblock_cache_size();

    int32_t _destroy_stream_store();
    int32_t _destroy_stream_store(StreamId_Ext& stream_id);

  protected:
    typedef __gnu_cxx::hash_map<StreamId_Ext, StreamStore*> StreamStoreMap_t;
    StreamStoreMap_t _stream_store_map;

    std::vector<CacheWatcher*> _notify_handle_vec;
    struct event _ev_timer;
    uint8_t		_module_type;
    struct event_base* _main_base;
    CacheManagerConfig*		_config;

    FlvCacheManagerStatistic *m_statistic;

    static FlvCacheManager* m_inst;
  };

  class CircularCache;
  class FlvCacheManagerStatistic {
  public:
    //void set_http_server(http::HTTPServer *server);
    void http_state(char* query, char* param, json_object* rsp);

  protected:
    typedef void (FlvCacheManagerStatistic::*HttpHandler_t)(char* query, char* param, json_object* rsp);
    void _init_http_server();
    void _state(char* query, char* param, json_object* rsp);
    void _stream_list(char* query, char* param, json_object* rsp);
    void _store_state(char* query, char* param, json_object* rsp);
    void _whitelist_fake(char* query, char* param, json_object* rsp);
    void _live_cache_state(CircularCache* cache, json_object* rsp);
    void _fragment_generator_state(fragment::FragmentGenerator* generator, json_object* rsp);
    void _flv_miniblock_generator_state(fragment::FLVMiniBlockGenerator* generator, json_object* rsp);
    void _flv_live_miniblock_cache_state(FLVMiniBlockCircularCache* cache, json_object* rsp);
    void _add_http_handler(const char* query, const char* param, HttpHandler_t handler);

    typedef std::map<std::string, HttpHandler_t> HttpHandlerMap_t;
    HttpHandlerMap_t _http_handler_map;
  };

}
