#include "media_manager/cache_manager.h"

#include "backend_new/module_backend.h"
#include "fragment/fragment_generator.h"
#include "media_manager/cache_manager_config.h"
#include "media_manager/circular_cache.h"
#include "media_manager/media_manager_state.h"
#include "rtp_trans/rtp_trans_manager.h"
#include "media_manager/rtp2flv_remuxer.h"

namespace media_manager {

  FlvCacheManager* FlvCacheManager::m_inst = NULL;

  FlvCacheManager* FlvCacheManager::Instance() {
    if (m_inst) {
      return m_inst;
    }
    m_inst = new FlvCacheManager();
    return m_inst;
  }

  void FlvCacheManager::DestroyInstance() {
    if (m_inst) {
      delete m_inst;
      m_inst = NULL;
    }
  }

  FlvCacheManager::FlvCacheManager() {
    _module_type = 0;
    _main_base = NULL;
    _config = NULL;
    m_statistic = new FlvCacheManagerStatistic();
  }

  FlvCacheManager::~FlvCacheManager() {
    for (auto it = _notify_handle_vec.begin(); it != _notify_handle_vec.end(); it++) {
      if ((*it) != NULL) {
        delete (*it);
      }
    }

    destroy_stream();

    if (_config != NULL) {
      delete _config;
      _config = NULL;
    }

    delete m_statistic;
    INF("~CacheManager()");
  }

  void FlvCacheManager::Init(event_base* base, uint8_t module_type, CacheManagerConfig* config /*= NULL*/) {
    _config = new CacheManagerConfig();
    _config->init(_module_type == MODULE_TYPE_UPLOADER);
    if (config) {
      load_config(config);
    }
    else {
      _config->load_default_config();
    }

    _main_base = base;
    StartTimer();
  }

  //void FlvCacheManager::set_http_server(http::HTTPServer *server) {
  //  m_statistic->set_http_server(server);
  //}

  int32_t FlvCacheManager::set_flv_header(StreamId_Ext stream_id, flv_header* input_flv_header, uint32_t flv_header_len) {
    INF("set_flv_header. streamid: %s", stream_id.unparse().c_str());

    StreamStore* stream_store = NULL;
    auto it = _stream_store_map.find(stream_id);
    if (it == _stream_store_map.end()) {
      stream_store = new StreamStore(stream_id, _module_type);
      _stream_store_map[stream_id] = stream_store;
    }
    else {
      stream_store = it->second;
    }

    int32_t header_len;
    if (stream_store->flv_miniblock_generator) {
      header_len = stream_store->flv_miniblock_generator->set_flv_header(input_flv_header, flv_header_len);
      if (header_len < 0)  {
        ERR("flv_miniblock_generator set flv header error, streamid: %s, header len: %d",
          stream_id.unparse().c_str(), header_len);
        assert(0);
        return header_len;
      }
    }

    header_len = stream_store->flv_miniblock_cache->set_flv_header(input_flv_header, flv_header_len);
    if (header_len < 0) {
      ERR("flv_miniblock_cache set flv header error, streamid: %s, header len: %d",
        stream_id.unparse().c_str(), header_len);
      assert(0);
      return header_len;
    }

    _notify_watcher(stream_id, CACHE_WATCHING_FLV_HEADER);
    stream_store->set_push_active();
    return header_len;
  }

  int32_t FlvCacheManager::set_flv_tag(StreamId_Ext stream_id, flv_tag* input_flv_tag) {
    StreamStore* stream_store = NULL;
    auto it = _stream_store_map.find(stream_id);
    if (it == _stream_store_map.end()) {
      ERR("cache manager doesn't contain this stream, id: %s", stream_id.unparse().c_str());
      assert(0);
      return STATUS_NO_THIS_STREAM;
    }
    else {
      stream_store = it->second;
    }

    bool generate_miniblock = false;
    stream_store->flv_miniblock_generator->set_flv_tag(input_flv_tag, generate_miniblock);
    if (generate_miniblock) {
      fragment::FLVMiniBlock* flv_block = stream_store->flv_miniblock_generator->get_block();
      if (flv_block == NULL) {
        ERR("flv_block is NULL from flv_miniblock_generator, stream: %s", stream_id.unparse().c_str());
      }
      else {
        int ret = stream_store->flv_miniblock_cache->push(flv_block);
        if (ret < 0) {
          delete flv_block;
        }
        else {
          stream_store->set_push_active();
          _notify_watcher(stream_id, CACHE_WATCHING_FLV_MINIBLOCK);
        }
      }
    }

    return 0;
  }

  int32_t FlvCacheManager::register_watcher(cache_watch_handler input_handler,
    uint8_t watch_type = CACHE_WATCHING_ALL, void* arg = NULL) {
    CacheWatcher* watcher = new CacheWatcher(input_handler, watch_type, arg);
    _notify_handle_vec.push_back(watcher);
    return _notify_handle_vec.size();
  }

  int32_t FlvCacheManager::_destroy_stream_store() {
    StreamStoreMap_t::iterator it;

    for (it = _stream_store_map.begin(); it != _stream_store_map.end(); it++) {
      StreamId_Ext stream_id = it->first;
      if (it->second == NULL) {
        ERR("StreamStore is null, stream id: %s", stream_id.unparse().c_str());
        continue;
      }

      // notify tracker
      //int32_t level = _get_tracker_level(stream_id); //#todo
      //backend_del_stream_from_tracker_v3(stream_id, -1); //#todo level
    }

    for (it = _stream_store_map.begin(); it != _stream_store_map.end(); it++) {
      StreamStore* stream_store = it->second;
      if (stream_store != NULL) {
        delete stream_store;
        stream_store = NULL;
        it->second = NULL;
      }
      _req_stop_from_backend(it->first);
    }

    _stream_store_map.clear();

    return 0;
  }

  int32_t FlvCacheManager::_destroy_stream_store(StreamId_Ext& stream_id) {
    StreamStoreMap_t::iterator it = _stream_store_map.find(stream_id);
    if (it == _stream_store_map.end()) {
      WRN("can not find stream %s in _stream_store_map, delete nothing", stream_id.unparse().c_str());
      return -1;
    }

    // notify tracker
    //int32_t level = _get_tracker_level(stream_id);//#todo
    //backend_del_stream_from_tracker_v3(stream_id, -1);//#todo level

    StreamStore* stream_store = it->second;
    if (stream_store != NULL) {
      delete stream_store;
      stream_store = NULL;
      it->second = NULL;
    }

    _stream_store_map.erase(it);

    _req_stop_from_backend(stream_id, CACHE_REQ_LIVE_FLV_LATEST_KEY_BLOCK);
    _req_stop_from_backend(stream_id, CACHE_REQ_LIVE_FLV_LATEST_KEY_FRAGMENT);
    _req_stop_from_backend(stream_id, CACHE_REQ_LIVE_FLV_LATEST_N_FRAGMENT);
    _req_stop_from_backend(stream_id, CACHE_REQ_LIVE_TS_LATEST_SEGMENT);
    _req_stop_from_backend(stream_id, CACHE_REQ_LIVE_RTP);

    RTP2FLVRemuxer::get_instance()->DestroyStream(stream_id);

    return 0;
  }

  int32_t FlvCacheManager::destroy_stream() {
    _destroy_stream_store();
    INF("destroy all stream");
    return 0;
  }

  int32_t FlvCacheManager::destroy_stream(StreamId_Ext stream_id) {
    _destroy_stream_store(stream_id);
    INF("destroy stream. stream_id %s.", stream_id.unparse().c_str());
    return 0;
  }

  bool FlvCacheManager::contains_stream(const StreamId_Ext& stream_id) {
    if (_stream_store_map.find(stream_id) != _stream_store_map.end()) {
      return true;
    }
    else {
      return false;
    }
  }

  int FlvCacheManager::get_miniblock_flv_header(StreamId_Ext stream_id, fragment::FLVHeader &header) {
    // INFO: zhangle, flv缓存和RTPTrans的生命周期不一致，因此需要HasUploader()补丁
    if (!RTPTransManager::Instance()->HasUploader(stream_id) || !contains_stream(stream_id)) {
      RelayManager::Instance()->StartPullRtp(stream_id);
      return -1;
    }

    //if (!contains_stream(stream_id)) {
    //  if (_module_type != MODULE_TYPE_UPLOADER) {
    //    INF("get_miniblock_flv_header failed, require from backend, streamid: %s", stream_id.unparse().c_str());
    //    //_req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_MINIBLOCK_HEADER);
    //    //_req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_LATEST_MINIBLOCK);
    //    // TODO: zhangle, 缓存和pull client的生命周期不一致会有问题，例如relay pull之后，pull client生命周期结束，close了rtp流，结果缓存中还有，不会再次pull
    //    RelayManager::Instance()->StartPullRtp(stream_id);
    //  }
    //  return -1;
    //}

    StreamStore* stream_store = _stream_store_map[stream_id];
    FLVMiniBlockCircularCache* cache = stream_store->flv_miniblock_cache;

    uint32_t header_len = 0;
    int32_t status_code = 0;
    flv_header* src_header = cache->get_flv_header(header_len, status_code);
    if (src_header) {
      INF("get_miniblock_flv_header success, streamid: %s, len: %u", stream_id.unparse().c_str(), header_len);
      stream_store->set_req_active();
      header.set_flv_header(src_header, header_len);
      return 0;
    }
    //else if (_module_type != MODULE_TYPE_UPLOADER) {
    //  INF("get_miniblock_flv_header failed, require from backend, streamid: %s", stream_id.unparse().c_str());
    //  assert(false);
    //}
    return -3;
  }

  fragment::FLVMiniBlock* FlvCacheManager::get_latest_miniblock(StreamId_Ext stream_id) {
    if (!contains_stream(stream_id)) {
      //if (_module_type != MODULE_TYPE_UPLOADER) {
      //  INF("get_latest_miniblock failed, require from backend, streamid: %s", stream_id.unparse().c_str());
      //  _req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_MINIBLOCK_HEADER);
      //  _req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_LATEST_MINIBLOCK);
      //}
      return NULL;
    }

    StreamStore* stream_store = _stream_store_map[stream_id];
    FLVMiniBlockCircularCache* cache = stream_store->flv_miniblock_cache;
    if (cache == NULL) {
      ERR("miniblock_cache is NULL, streamid: %s", stream_id.unparse().c_str());
      return NULL;
    }

    // INFO: zhangle, change "get_latest" to "get_latest_key", is it fit flv_publish?
    int32_t status_code = 0;
    fragment::FLVMiniBlock* block = dynamic_cast<fragment::FLVMiniBlock*>(cache->get_latest_key(status_code));
    if (block) {
      INF("get_latest_miniblock success, streamid: %s, seq: %u", stream_id.unparse().c_str(), block->get_seq());
      stream_store->set_req_active();
      return block;
    }
    //else if (_module_type != MODULE_TYPE_UPLOADER) {
    //  INF("get_latest_miniblock failed, require from backend, streamid: %s", stream_id.unparse().c_str());
    //  int32_t status = 0;
    //  uint32_t header_len = 0;
    //  if (cache->get_flv_header(header_len, status, true) == NULL) {
    //    _req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_MINIBLOCK_HEADER);
    //  }
    //  _req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_LATEST_MINIBLOCK);
    //}
    return NULL;
  }

  fragment::FLVMiniBlock* FlvCacheManager::get_miniblock_by_seq(StreamId_Ext stream_id, int32_t seq) {
    if (!contains_stream(stream_id)) {
      return NULL;
    }

    StreamStore* stream_store = _stream_store_map[stream_id];
    FLVMiniBlockCircularCache* cache = stream_store->flv_miniblock_cache;
    if (cache == NULL) {
      ERR("miniblock_cache is NULL, streamid: %s", stream_id.unparse().c_str());
      return NULL;
    }

    int32_t status_code = 0;
    fragment::FLVMiniBlock* block = dynamic_cast<fragment::FLVMiniBlock*>(cache->get_by_seq(seq, status_code));
    if (block) {
      stream_store->set_req_active();
      return block;
    }
    else {
      return NULL;
    }
  }

  int32_t FlvCacheManager::load_config(const CacheManagerConfig* config) {
    *_config = *config;
    return 0;
  }

  // protected method:
  int32_t FlvCacheManager::_notify_watcher(StreamId_Ext& streamid, uint8_t watch_type) {
    int32_t len = _notify_handle_vec.size();
    for (int32_t i = 0; i < len; i++) {
      CacheWatcher* watcher = _notify_handle_vec[i];
      cache_watch_handler handler = watcher->handler;

      if (handler && (watcher->watch_type & watch_type)) {
        handler(streamid, watch_type, watcher->subscriber);
      }
    }

    return len;
  }

  int32_t FlvCacheManager::_req_from_backend(StreamId_Ext stream_id, int32_t request_state, int32_t seq) {
    return 0;
  }

  int32_t FlvCacheManager::_req_stop_from_backend(StreamId_Ext stream_id, int32_t request_state, int32_t seq) {
    //if (_module_type == MODULE_TYPE_UPLOADER) {
    //  return 0;
    //}

    if (request_state == CACHE_REQ_LIVE_RTP) {
      _req_stop_from_backend_rtp(stream_id);
      return 0;
    }

    return 0;
  }

  int32_t FlvCacheManager::_req_stop_from_backend(uint32_t stream_id) {
    return 0;
  }

  int32_t FlvCacheManager::_req_stop_from_backend_rtp(StreamId_Ext stream_id)
  {
    RelayManager::Instance()->StopPullRtp(stream_id);
    return 0;
  }

  int32_t FlvCacheManager::_req_stop_from_backend(StreamId_Ext stream_id) {
    return 0;
  }

  void FlvCacheManager::StartTimer() {
    struct timeval tv;
    evtimer_set(&_ev_timer, OnTimer, (void *)this);
    tv.tv_sec = _config->live_push_timeout_sec;
    tv.tv_usec = 0;
    event_base_set(_main_base, &_ev_timer);
    evtimer_add(&_ev_timer, &tv);
  }

  void FlvCacheManager::OnTimer(const int32_t fd, short which, void *arg) {
    FlvCacheManager *pThis = (FlvCacheManager*)arg;
    pThis->OnTimerImpl();
  }

  void FlvCacheManager::OnTimerImpl() {
    _check_stream_store_timeout();
    _adjust_flv_miniblock_cache_size();
    StartTimer();
  }

  static std::vector<StreamId_Ext> temp_remove_block_vec;

  void FlvCacheManager::_check_stream_store_timeout() {
    //if (_module_type == MODULE_TYPE_UPLOADER) {
    //  return;
    //}

    time_t now = time(NULL);

    temp_remove_block_vec.clear();

    StreamStoreMap_t::iterator hash_map_it;

    for (hash_map_it = _stream_store_map.begin();
      hash_map_it != _stream_store_map.end(); hash_map_it++) {
      StreamStore* stream_store = (*hash_map_it).second;

      if (stream_store != NULL) {
        // we will clear this stream in forward and player if there is long time no output data.

        time_t last_req_time = stream_store->get_push_active_time();
        // TODO: zhangle, should set by config file
        _config->live_req_timeout_sec = 20;
        if (_config->live_req_timeout_sec > 0) {
          if (/*_module_type != MODULE_TYPE_UPLOADER && */now - last_req_time >= _config->live_req_timeout_sec)
          {
            INF("remove stream: %s from cache manager because long time no request.",
              stream_store->stream_id.unparse().c_str());
            temp_remove_block_vec.push_back(stream_store->stream_id);
            continue;
          }
        }

        // we will require this stream again in forward and player if there is long time no input data.
        // 0. flv miniblock
        //if (stream_store->flv_miniblock_cache != NULL) {
        //  time_t last_push_time = stream_store->flv_miniblock_cache->get_push_active_time();

        //  if (_module_type != MODULE_TYPE_UPLOADER
        //    && last_push_time != 0
        //    && now - last_push_time >= _config->live_push_timeout_sec)
        //  {
        //    WRN("flv_miniblock_cache long time no data for stream: %s, from: %ld, now: %ld, request from backend again.",
        //      stream_store->stream_id.unparse().c_str(), last_push_time, now);
        //    _req_from_backend((*hash_map_it).first, CACHE_REQ_LIVE_FLV_LATEST_MINIBLOCK);
        //  }
        //}
      }
    }

    int num = temp_remove_block_vec.size();
    for (int i = 0; i < num; i++) {
      destroy_stream(temp_remove_block_vec[i]);
    }
    temp_remove_block_vec.clear();
  }

  void FlvCacheManager::_adjust_flv_miniblock_cache_size() {
    StreamStoreMap_t::iterator store_it = _stream_store_map.begin();
    for (; store_it != _stream_store_map.end(); store_it++) {
      StreamStore* stream_store = store_it->second;
      FLVMiniBlockCircularCache* cache = stream_store->flv_miniblock_cache;
      if (cache == NULL) {
        continue;
      }

      int size = cache->size();
      while (cache->size() > 0) {
        int32_t status;
        fragment::BaseBlock* back_block = cache->back(status);
        fragment::BaseBlock* front_block = cache->front(status);
        int32_t max_duration = _config->flv_block_live_cache->buffer_duration_sec * 1000;
        int32_t max_size = _config->flv_block_live_cache->buffer_duration_sec * 100;

        if (((int32_t)back_block->get_start_ts() - (int32_t)front_block->get_start_ts() > max_duration)
          || ((int32_t)back_block->get_seq() - (int32_t)front_block->get_seq() > max_size)) {
          TRC("flv_miniblock_cache pop FLVBlock, stream: %s, seq: %d",
            front_block->get_stream_id().unparse().c_str(), front_block->get_seq());
          cache->pop_front(true);
        }
        else {
          break;
        }
      }

      DBG("_adjust_flv_miniblock_cache_size, streamid: %s, before: %d, after %d",
        cache->stream_id().unparse().c_str(), size, cache->size());
    }
  }

}
