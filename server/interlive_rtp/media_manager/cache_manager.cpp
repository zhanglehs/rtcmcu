/**
* @file cache_manager.cpp
* @brief implementation of cache_manager.h
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/05/29
* @see  fragment.h \n
*         cache_manager.h
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
#include "cache_manager.h"
#include "../backend_new/module_backend.h"
#include "../backend_new/forward_common.h"
#include "util/flv.h"
//#include "http_server.h"

using namespace std;
using namespace fragment;
using namespace avformat;

namespace media_manager
{
  static void cache_timer_service(const int32_t fd, short which, void *arg)
  {
    CacheManager* cache = (CacheManager*)arg;
    cache->timer_service(fd, which, arg);
  }

  CacheManager::CacheManager(uint8_t module_type, WhiteListMap* white_list_map)
    :_module_type(module_type),
    _white_list_map(white_list_map)
  {
    if (_instance != NULL)
    {
      return;
    }

    _config = new CacheManagerConfig();
    _config->init(_module_type == MODULE_TYPE_UPLOADER);
    _config->load_default_config();
    _time_service_active = false;
    _init_http_server();

    _instance = this;
  }

  CacheManager::CacheManager(uint8_t module_type, CacheManagerConfig* config, WhiteListMap* white_list_map)
    :_module_type(module_type),
    _white_list_map(white_list_map)
  {
    if (_instance != NULL)
    {
      return;
    }

    _config = new CacheManagerConfig();
    _config->init(_module_type == MODULE_TYPE_UPLOADER);
    load_config(config);
    _time_service_active = false;
    _init_http_server();

    _instance = this;
  }

  void CacheManager::set_event_base(event_base* base)
  {
    _main_base = base;
  }

  CacheManager::~CacheManager()
  {
    _http_handler_map.clear();

    vector<CacheWatcher*>::iterator _notify_handle_vec_it;

    for (_notify_handle_vec_it = _notify_handle_vec.begin();
      _notify_handle_vec_it != _notify_handle_vec.end();
      _notify_handle_vec_it++)
    {
      if ((*_notify_handle_vec_it) != NULL)
      {
        delete (*_notify_handle_vec_it);
      }
    }

    destroy_stream();

    if (_config != NULL)
    {
      delete _config;
      _config = NULL;
    }

    INF("~CacheManager()");
    _instance = NULL;
  }

  void CacheManager::Destroy()
  {
    if (_instance != NULL)
    {
      delete _instance;
      _instance = NULL;
    }
  }

  CacheManager* CacheManager::_instance = NULL;

  UploaderCacheManagerInterface* CacheManager::get_uploader_cache_instance()
  {
    return get_cache_manager();
  }

  PlayerCacheManagerInterface* CacheManager::get_player_cache_instance()
  {
    return get_cache_manager();
  }

  MediaManagerRTPInterface* CacheManager::get_rtp_cache_instance()
  {
    return get_cache_manager();
  }

  CacheManager* CacheManager::get_cache_manager()
  {
    return _instance;
  }

  // UploaderCacheManagerInterface
  int32_t CacheManager::set_flv_header(StreamId_Ext stream_id, flv_header* input_flv_header, uint32_t flv_header_len)
  {
    int32_t status_code = 0;

    INF("set_flv_header. streamid: %s", stream_id.unparse().c_str());
    if (!contains_stream(stream_id, status_code))
    {
      WRN("cache manager doesn't contain this stream, id: %s", stream_id.unparse().c_str());
      return status_code;
    }

    StreamStore* stream_store = _stream_store_map[stream_id];

    if (stream_store == NULL)
    {
      ERR("stream_store is null, streamid: %s", stream_id.unparse().c_str());
      assert(0);
      return -1;
    }

    if (stream_store->state == STREAM_STORE_CONSTRUCT)
    {
      stream_store->init(_module_type, _config, this);
    }

    if (stream_store->state != STREAM_STORE_INIT
      && stream_store->state != STREAM_STORE_ACTIVE)
    {
      ERR("stream_store state error, streamid: %s, state: %d",
        stream_id.unparse().c_str(), stream_store->state);
      assert(0);
      return -1;
    }

    stream_store->state = STREAM_STORE_ACTIVE;

    int32_t header_len;
    if (_module_type == MODULE_TYPE_UPLOADER)
    {
      header_len = stream_store->flv_miniblock_generator->set_flv_header(input_flv_header, flv_header_len);
      if (header_len < 0)
      {
        ERR("flv_miniblock_generator set flv header error, streamid: %s, header len: %d",
          stream_id.unparse().c_str(), header_len);
        assert(0);
        return header_len;
      }
    }


    header_len = stream_store->flv_miniblock_cache->set_flv_header(input_flv_header, flv_header_len);
    if (header_len < 0)
    {
      ERR("flv_miniblock_cache set flv header error, streamid: %s, header len: %d",
        stream_id.unparse().c_str(), header_len);
      assert(0);
      return header_len;
    }

    _notify_watcher(stream_id, CACHE_WATCHING_FLV_HEADER);
    stream_store->set_push_active();

    return header_len;
  }

  int32_t CacheManager::set_flv_tag(StreamId_Ext stream_id, flv_tag* input_flv_tag, bool malloc_new_memory_flag)
  {
    int32_t status_code = 0;
    if (!contains_stream(stream_id, status_code))
    {
      ERR("cache manager doesn't contain this stream, id: %s", stream_id.unparse().c_str());
      assert(0);
      return status_code;
    }

    uint8_t watch_type = 0;

    StreamStore* stream_store = _stream_store_map[stream_id];

    if (stream_store == NULL)
    {
      ERR("stream_store is null, streamid: %s", stream_id.unparse().c_str());
      assert(0);
      return -1;
    }

    if (stream_store->state == STREAM_STORE_CONSTRUCT)
    {
      stream_store->init(_module_type, _config, this);
    }

    if (stream_store->state != STREAM_STORE_ACTIVE)
    {
      ERR("stream_store state error, streamid: %s, state: %d",
        stream_id.unparse().c_str(), stream_store->state);
      assert(0);
      return -1;
    }

    bool generate_miniblock = false;
    stream_store->flv_miniblock_generator->set_flv_tag(input_flv_tag, generate_miniblock);
    if (generate_miniblock)
    {
      watch_type |= CACHE_WATCHING_FLV_MINIBLOCK;
      FLVMiniBlock* flv_block = stream_store->flv_miniblock_generator->get_block();
      if (flv_block == NULL)
      {
        ERR("flv_block is NULL from flv_miniblock_generator, stream: %s", stream_id.unparse().c_str());
      }
      else
      {
        int ret = stream_store->flv_miniblock_cache->push(flv_block);
        if (ret < 0)
        {
          delete flv_block;
        }
      }
    }

    if (generate_miniblock)
    {
      stream_store->set_push_active();
      _notify_watcher(stream_id, watch_type);
    }

    return 0;
  }

  int32_t CacheManager::register_watcher(cache_watch_handler input_handler,
    uint8_t watch_type = CACHE_WATCHING_ALL, void* arg = NULL)
  {
    CacheWatcher* watcher = new CacheWatcher(input_handler, watch_type, arg);
    _notify_handle_vec.push_back(watcher);
    return _notify_handle_vec.size();
  }

  int32_t CacheManager::add_tracker_info(StreamId_Ext stream_id, uint32_t parent_ip, uint16_t parent_port, uint16_t my_level)
  {
    int32_t status = 0;
    int32_t rtn = 0;

    if (!contains_stream(stream_id, status))
    {
      rtn = init_stream(stream_id, status);
      if (status == STATUS_SUCCESS)
      {
        INF("init stream for add_tracker info.");
      }
      else
      {
        ERR("init stream failed.");
      }
    }

    INF("add_tracker_info for stream %s, parent ip %s, parent port %u, my_level %u.",
      stream_id.unparse().c_str(), util_ip2str(parent_ip), parent_port, my_level);

    return rtn;
  }

  int32_t CacheManager::stop_stream(StreamId_Ext stream_id)
  {
    destroy_stream(stream_id);

    return -1;
  }

  int32_t CacheManager::_destroy_stream_store()
  {
    StreamStoreMap_t::iterator it;

    for (it = _stream_store_map.begin(); it != _stream_store_map.end(); it++)
    {
      StreamId_Ext stream_id = it->first;
      if (it->second == NULL)
      {
        ERR("StreamStore is null, stream id: %s", stream_id.unparse().c_str());
        continue;
      }

      // notify tracker
      //int32_t level = _get_tracker_level(stream_id); //#todo
      backend_del_stream_from_tracker_v3(stream_id, -1); //#todo level
    }

    for (it = _stream_store_map.begin(); it != _stream_store_map.end(); it++)
    {
      StreamStore* stream_store = it->second;
      if (stream_store != NULL)
      {
        delete stream_store;
        stream_store = NULL;
        it->second = NULL;
      }
      _req_stop_from_backend(it->first);
    }

    _stream_store_map.clear();

    return 0;
  }

  int32_t CacheManager::_destroy_stream_store(StreamId_Ext& stream_id)
  {
    StreamStoreMap_t::iterator it = _stream_store_map.find(stream_id);

    if (it == _stream_store_map.end())
    {
      WRN("can not find stream %s in _stream_store_map, delete nothing", stream_id.unparse().c_str());
      return -1;
    }

    // notify tracker
    //int32_t level = _get_tracker_level(stream_id);//#todo

    backend_del_stream_from_tracker_v3(stream_id, -1);//#todo level

    StreamStore* stream_store = it->second;
    if (stream_store != NULL)
    {
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

    return 0;
  }

  int32_t CacheManager::destroy_stream()
  {
    _destroy_stream_store();
    INF("destroy all stream");

    return 0;
  }

  int32_t CacheManager::destroy_stream(StreamId_Ext stream_id)
  {
    _destroy_stream_store(stream_id);
    INF("destroy stream. stream_id %s.", stream_id.unparse().c_str());

    return 0;
  }

  int32_t CacheManager::stop_stream(uint32_t stream_id)
  {
    destroy_stream(stream_id);

    return -1;
  }

  int32_t CacheManager::_destroy_stream_store(uint32_t stream_id)
  {
    // destroy this stream in block_cache.
    list<StreamId_Ext> remove_list;

    _req_stop_from_backend(stream_id);

    for (StreamStoreMap_t::iterator it = _stream_store_map.begin();
      it != _stream_store_map.end(); it++)
    {
      if (it->second == NULL)
      {
        ERR("item is none in _block_cache_map, stream_id %s", it->first.unparse().c_str());
        assert(0);
        continue;
      }

      if (it->first.get_32bit_stream_id() == stream_id)
      {
        remove_list.push_back(it->first);
      }
    }

    for (list<StreamId_Ext>::iterator it = remove_list.begin(); it != remove_list.end(); it++)
    {
      _destroy_stream_store(*it);
    }

    return remove_list.size();
  }

  int32_t CacheManager::destroy_stream(uint32_t stream_id)
  {
    _destroy_stream_store(stream_id);
    INF("destroy stream. stream_id %d.", stream_id);

    return 0;
  }

  // PlayerCacheManagerInterface
  bool CacheManager::contains_stream(const StreamId_Ext& stream_id, int32_t& status_code)
  {
    if (_stream_store_map.find(stream_id) != _stream_store_map.end())
    {
      return true;
    }
    else
    {
      status_code = STATUS_NO_THIS_STREAM;
      return false;
    }
  }

  bool CacheManager::contains_stream(const StreamId_Ext& stream_id)
  {
    if (_stream_store_map.find(stream_id) != _stream_store_map.end())
    {
      return true;
    }
    else
    {
      return false;
    }
  }

  bool CacheManager::stream_initialized(StreamId_Ext& stream_id)
  {
    if (_stream_store_map.find(stream_id) != _stream_store_map.end())
    {
      if (_stream_store_map[stream_id]->state == STREAM_STORE_CONSTRUCT)
      {
        return false;
      }
      else
      {
        return true;
      }
    }
    else
    {
      return false;
    }
  }

  flv_header* CacheManager::get_miniblock_flv_header(StreamId_Ext stream_id, uint32_t& header_len, int32_t& status_code, bool req_from_backend)
  {
    if (!contains_stream(stream_id, status_code))
    {
      header_len = 0;

      if ((status_code == STATUS_NO_THIS_STREAM)
        && _module_type != MODULE_TYPE_UPLOADER
        && req_from_backend)
      {
        status_code = STATUS_REQ_DATA;
        INF("get_miniblock_flv_header failed, require from backend, streamid: %s", stream_id.unparse().c_str());
        _req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_MINIBLOCK_HEADER);
        _req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_LATEST_MINIBLOCK);
      }
      return NULL;
    }

    StreamStore* stream_store = _stream_store_map[stream_id];
    FLVMiniBlockCircularCache* cache = stream_store->flv_miniblock_cache;
    if (cache == NULL)
    {
      ERR("block_cache is NULL, streamid: %s", stream_id.unparse().c_str());
      status_code = STATUS_NO_DATA;
      return NULL;
    }

    flv_header* header = cache->get_flv_header(header_len, status_code);
    switch (status_code)
    {
    case STATUS_SUCCESS:
      INF("get_miniblock_flv_header success, streamid: %s, len: %u", stream_id.unparse().c_str(), header_len);
      stream_store->set_req_active();
      return header;

    case STATUS_NO_DATA:
      if (_module_type != MODULE_TYPE_UPLOADER && req_from_backend)
      {
        INF("get_miniblock_flv_header failed, require from backend, streamid: %s", stream_id.unparse().c_str());
        status_code = STATUS_REQ_DATA;
        header_len = 0;
        _req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_MINIBLOCK_HEADER);
        _req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_LATEST_MINIBLOCK);
      }
      return NULL;

    default:
      return NULL;
    }
  }

  FLVHeader* CacheManager::get_miniblock_flv_header(StreamId_Ext stream_id, FLVHeader &header, int32_t& status_code)
  {
    uint32_t header_len = 0;
    flv_header* src_header = get_miniblock_flv_header(stream_id, header_len, status_code, true);
    if (src_header && header_len > 0)
    {
      header.set_flv_header(src_header, header_len);
      return &header;
    }
    return NULL;
  }

  FLVMiniBlock* CacheManager::get_latest_miniblock(StreamId_Ext stream_id, int32_t& status_code, bool req_from_backend)
  {
    if (!contains_stream(stream_id, status_code))
    {
      if ((status_code == STATUS_NO_THIS_STREAM) && _module_type != MODULE_TYPE_UPLOADER && req_from_backend)
      {
        INF("get_latest_miniblock failed, require from backend, streamid: %s", stream_id.unparse().c_str());
        status_code = STATUS_REQ_DATA;
        _req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_MINIBLOCK_HEADER);
        _req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_LATEST_MINIBLOCK);
      }
      return NULL;
    }

    StreamStore* stream_store = _stream_store_map[stream_id];
    FLVMiniBlockCircularCache* cache = stream_store->flv_miniblock_cache;
    if (cache == NULL)
    {
      ERR("miniblock_cache is NULL, streamid: %s", stream_id.unparse().c_str());
      status_code = STATUS_NO_DATA;
      return NULL;
    }

    // INFO: zhangle, change "get_latest" to "get_latest_key", is it fit flv_publish?
    FLVMiniBlock* block = dynamic_cast<FLVMiniBlock*>(cache->get_latest_key(status_code));
    switch (status_code)
    {
    case STATUS_SUCCESS:
      INF("get_latest_miniblock success, streamid: %s, seq: %u", stream_id.unparse().c_str(), block->get_seq());
      stream_store->set_req_active();
      return block;

    case STATUS_NO_DATA:
      if (_module_type != MODULE_TYPE_UPLOADER && req_from_backend)
      {
        INF("get_latest_miniblock failed, require from backend, streamid: %s", stream_id.unparse().c_str());
        status_code = STATUS_REQ_DATA;
        int32_t status = 0;
        uint32_t header_len = 0;
        if (cache->get_flv_header(header_len, status, true) == NULL)
        {
          _req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_MINIBLOCK_HEADER);
        }
        _req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_LATEST_MINIBLOCK);
      }
      return NULL;

    default:
      return NULL;
    }
  }

  FLVMiniBlock* CacheManager::get_miniblock_by_seq(StreamId_Ext stream_id, int32_t seq, int32_t& status_code, bool return_next_if_not_found, bool req_from_backend)
  {
    if (!contains_stream(stream_id, status_code))
    {
      return NULL;
    }

    StreamStore* stream_store = _stream_store_map[stream_id];
    FLVMiniBlockCircularCache* cache = stream_store->flv_miniblock_cache;
    if (cache == NULL)
    {
      ERR("miniblock_cache is NULL, streamid: %s", stream_id.unparse().c_str());
      status_code = STATUS_NO_DATA;
      return NULL;
    }

    FLVMiniBlock* block = dynamic_cast<FLVMiniBlock*>(cache->get_by_seq(seq, status_code, return_next_if_not_found));
    if (status_code == STATUS_SUCCESS)
    {
      stream_store->set_req_active();
      return block;
    }
    else
    {
      return NULL;
    }
  }

  SdpInfo* CacheManager::get_sdp(StreamId_Ext stream_id, int32_t& status_code, bool req_from_backend)
  {
    RTPMediaCache* media_cache = get_rtp_media_cache(stream_id, status_code, req_from_backend);

    if (media_cache == NULL)
    {
      INF("media cache is null, stream: %s, status: %d", stream_id.unparse().c_str(),
        status_code);
      return NULL;
    }

    SdpInfo* sdp = media_cache->get_sdp(status_code);

    switch (status_code)
    {
    case STATUS_SUCCESS:
      return sdp;

    case STATUS_NO_DATA:
      if (_module_type != MODULE_TYPE_UPLOADER && req_from_backend)
      {
        status_code = STATUS_REQ_DATA;
      }
      break;

    default:
      break;
    }

    if (sdp == NULL)
    {
      INF("media cache get sdp failed, stream: %s", stream_id.unparse().c_str());
      return NULL;
    }

    return sdp;
  }

  int32_t CacheManager::init_stream(const StreamId_Ext& stream_id, int32_t& status_code)
  {
    if (contains_stream(stream_id, status_code))
    {
      status_code = STATUS_REINIT_STREAM;
      ERR("stream was init already. stream_id: %s", stream_id.unparse().c_str());
      return 0;
    }

    // 白名单机制被我disable了，所以注释掉这段
    //StreamId_Ext whitelist_stream_id = StreamId_Ext(stream_id.get_32bit_stream_id(), 0);
    //WhiteListMap::iterator it = _white_list_map->find(whitelist_stream_id);
    //if (it == _white_list_map->end())
    //{
    //  status_code = STATUS_STREAM_ID_INVALID;
    //  ERR("stream is invalid. stream_id: %s", stream_id.unparse().c_str());
    //  return -1;
    //}

    StreamStore* stream_store = new StreamStore(stream_id);
    stream_store->init(_module_type, _config, this);
    _stream_store_map[stream_id] = stream_store;

    INF("CacheManager init_stream, stream id: %s", stream_id.unparse().c_str());
    status_code = STATUS_SUCCESS;

    return 0;
  }

  int32_t CacheManager::init_stream(const StreamId_Ext& stream_id)
  {
    int32_t status_code = 0;
    return init_stream(stream_id, status_code);
  }

  int32_t CacheManager::set_flv_miniblock_flvheader(StreamId_Ext stream_id, flv_header* input_flv_header, uint32_t flv_header_len)
  {
    int32_t status_code = 0;
    if (!contains_stream(stream_id, status_code))
    {
      WRN("set_flv_miniblock_flvheader failed, stream %s not exist", stream_id.unparse().c_str());
      return -1;
    }

    StreamStore* stream_store = _stream_store_map[stream_id];
    if (stream_store == NULL)
    {
      ERR("stream_store is null, streamid: %s", stream_id.unparse().c_str());
      assert(0);
      return -1;
    }

    if (stream_store->state == STREAM_STORE_CONSTRUCT)
    {
      stream_store->init(_module_type, _config, this);
    }

    if (stream_store->state != STREAM_STORE_INIT && stream_store->state != STREAM_STORE_ACTIVE)
    {
      ERR("stream_store state error, streamid: %s, state: %d",
        stream_id.unparse().c_str(), stream_store->state);
      assert(0);
      return -1;
    }

    int32_t header_len = stream_store->flv_miniblock_cache->set_flv_header(input_flv_header, flv_header_len);
    if (header_len < 0)
    {
      ERR("flv_miniblock_cache set flv header error, streamid: %s, header len: %d",
        stream_id.unparse().c_str(), header_len);
      assert(0);
      return header_len;
    }

    _notify_watcher(stream_id, CACHE_WATCHING_FLV_MINIBLOCK);
    stream_store->set_push_active();

    return header_len;
  }

  int32_t CacheManager::set_miniblock(fragment::flv_miniblock_header* input_block)
  {
    StreamId_Ext stream_id = input_block->stream_id;

    int32_t status_code = 0;

    if (!contains_stream(stream_id, status_code))
    {
      WRN("set_miniblock failed, stream %s not exist", stream_id.unparse().c_str());
      return -1;
    }

    StreamStore* stream_store = _stream_store_map[stream_id];
    if (stream_store == NULL)
    {
      ERR("stream_store is null, streamid: %s", stream_id.unparse().c_str());
      assert(0);
      return -1;
    }

    if (stream_store->state == STREAM_STORE_CONSTRUCT)
    {
      stream_store->init(_module_type, _config, this);
    }

    if (stream_store->state != STREAM_STORE_INIT && stream_store->state != STREAM_STORE_ACTIVE)
    {
      ERR("stream_store state error, streamid: %s, state: %d",
        stream_id.unparse().c_str(), stream_store->state);
      assert(0);
      return -1;
    }

    FLVMiniBlockCircularCache* cache = stream_store->flv_miniblock_cache;
    uint32_t flv_header_len;
    if (cache->get_flv_header(flv_header_len, status_code, true) == NULL)
    {
      ERR("no flv header in block_cache, stream %s", stream_id.unparse().c_str());
      _req_from_backend(stream_id, CACHE_REQ_LIVE_FLV_MINIBLOCK_HEADER);
    }

    FLVMiniBlock* block = new FLVMiniBlock(input_block, true);
    int ret = cache->push(block);
    if (ret < 0)
    {
      delete block;
      return -1;
    }

    stream_store->set_push_active();
    _notify_watcher(stream_id, CACHE_WATCHING_FLV_MINIBLOCK);

    return 1;
  }

  int32_t isdir(const char *path)
  {
    struct stat buf;
    int32_t cc;
    cc = stat(path, &buf);
    if (!cc && (buf.st_mode & S_IFDIR)) return(1);
    return(cc);
  }

  int32_t CacheManager::_init_dump_stream_dir(StreamId_Ext stream_id)
  {

    string save_stream_dir(_config->save_stream_dir);
    if (isdir(_config->save_stream_dir) != 1)
    {
      WRN("failed to find dump_stream dir.");

      string mk_dir = "mkdir " + save_stream_dir;
      ::system(mk_dir.c_str());
    }

    string stream_id_dir = stream_id.unparse();
    stream_id_dir = save_stream_dir + "/" + stream_id_dir;
    if (isdir(stream_id_dir.c_str()) == 1)
    {
      string rm_dir = "rm -rf " + stream_id_dir;
      ::system(rm_dir.c_str());
    }

    string mk_dir = "mkdir " + stream_id_dir;
    ::system(mk_dir.c_str());

    char stream_char[4096];

    ::sprintf(stream_char, "mkdir %s/flv", stream_id_dir.c_str());
    ::system(stream_char);

    ::sprintf(stream_char, "mkdir %s/header", stream_id_dir.c_str());
    ::system(stream_char);

    return 0;
  }

  int32_t CacheManager::load_config(const CacheManagerConfig* config)
  {
    *_config = *config;

    return 0;
  }

  void CacheManager::timer_service(const int32_t fd, short which, void *arg)
  {
    _check_stream_store_timeout();

    _adjust_flv_miniblock_cache_size();

    start_timer();

  }

  RTPMediaCache* CacheManager::get_rtp_media_cache(const StreamId_Ext& stream_id, int32_t& status_code, bool req_from_backend)
  {
    if (!contains_stream(stream_id, status_code))
    {
      if ((status_code == STATUS_NO_THIS_STREAM)
        && _module_type != MODULE_TYPE_UPLOADER
        && req_from_backend)
      {
        status_code = STATUS_REQ_DATA;
        //_req_from_backend(stream_id, CACHE_REQ_LIVE_SDP);
        _req_from_backend_rtp(stream_id, CACHE_REQ_LIVE_RTP);

        return NULL;
      }

      if (!req_from_backend)
      {
        int ret = init_stream(stream_id, status_code);
        if (ret < 0)
        {
          ERR("init stream failed, stream_id: %s", stream_id.unparse().c_str());
          return NULL;
        }
      }
    }

    StreamStoreMap_t::iterator it = _stream_store_map.find(stream_id);

    if (it == _stream_store_map.end())
    {
      ERR("stream_store is NULL, streamid: %s", stream_id.unparse().c_str());
      status_code = STATUS_NO_DATA;
      return NULL;
    }

    StreamStore* stream_store = it->second;

    RTPMediaCache* cache = stream_store->rtp_media_cache;
    if (cache == NULL)
    {
      ERR("rtp_media_cache is NULL, streamid: %s", stream_id.unparse().c_str());
      status_code = STATUS_NO_DATA;
      return NULL;
    }

    if (req_from_backend && _module_type != MODULE_TYPE_UPLOADER)
    {
      if (cache->get_sdp(status_code) == NULL || cache->rtp_empty())
      {
        status_code = STATUS_REQ_DATA;
        //_req_from_backend(stream_id, CACHE_REQ_LIVE_SDP);
        _req_from_backend_rtp(stream_id, CACHE_REQ_LIVE_RTP);
      }
    }

    status_code = STATUS_SUCCESS;
    return cache;
  }

  StreamStore* CacheManager::get_stream_store(StreamId_Ext& stream_id, int32_t& status_code)
  {
    StreamStoreMap_t::iterator it = _stream_store_map.find(stream_id);

    if (it == _stream_store_map.end())
    {
      status_code = STATUS_NO_DATA;
      return NULL;
    }

    StreamStore* stream_store = it->second;
    return stream_store;
  }

  void CacheManager::on_timer()
  {
    timer_service(0, 0, NULL);
  }

  int32_t CacheManager::notify_watcher(StreamId_Ext& stream_id, uint8_t watch_type)
  {
    return _notify_watcher(stream_id, watch_type);
  }

  // protected method:
  int32_t CacheManager::_notify_watcher(StreamId_Ext& streamid, uint8_t watch_type)
  {
    int32_t len = _notify_handle_vec.size();
    for (int32_t i = 0; i < len; i++)
    {
      CacheWatcher* watcher = _notify_handle_vec[i];
      cache_watch_handler handler = watcher->handler;

      if (handler && (watcher->watch_type & watch_type))
      {
        handler(streamid, watch_type, watcher->subscriber);
      }
    }

    return len;
  }


  int32_t CacheManager::_get_cache_request_status(int32_t status_code)
  {
    int32_t cache_req_status;

    switch (status_code)
    {
    case STATUS_SUCCESS:
      cache_req_status = STATUS_CACHE_REQ_OK;
      break;

    case STATUS_PARTIAL_CONTENT:
      cache_req_status = STATUS_CACHE_REQ_PARTIAL_CONTENT;
      break;

    case STATUS_REQ_DATA:
      cache_req_status = STATUS_CACHE_REQ_ACCEPT;
      break;

    case STATUS_NO_DATA:
      cache_req_status = STATUS_CACHE_REQ_NOT_FOUND;
      break;

    case STATUS_DATA_NOT_COMPLETE:
      cache_req_status = STATUS_CACHE_REQ_NOT_FOUND;
      break;

    case STATUS_STREAM_STOPPED:
      cache_req_status = STATUS_CACHE_REQ_NOT_FOUND;
      break;

    case STATUS_NO_THIS_STREAM:
      cache_req_status = STATUS_CACHE_REQ_NOT_FOUND;
      break;

    case STATUS_STREAM_ID_INVALID:
      cache_req_status = STATUS_CACHE_REQ_BAD_REQUEST;
      break;

    case STATUS_TOO_SMALL:
      cache_req_status = STATUS_CACHE_REQ_NOT_FOUND;
      break;

    case STATUS_TOO_LARGE:
      cache_req_status = STATUS_CACHE_REQ_NOT_FOUND;
      break;

    case STATUS_PARAMETER_INVALID:
      cache_req_status = STATUS_CACHE_REQ_BAD_REQUEST;
      break;

    case STATUS_RAW_DATA_INVALID:
      cache_req_status = STATUS_CACHE_REQ_BAD_REQUEST;
      break;

    case STATUS_RAW_DATA_OVER_SIZE:
      cache_req_status = STATUS_RAW_DATA_OVER_SIZE;
      break;

    case STATUS_CACHE_REQ_OK:
      cache_req_status = STATUS_CACHE_REQ_OK;
      break;

    case STATUS_CACHE_REQ_ACCEPT:
      cache_req_status = STATUS_CACHE_REQ_ACCEPT;
      break;

    case STATUS_CACHE_REQ_PARTIAL_CONTENT:
      cache_req_status = STATUS_CACHE_REQ_PARTIAL_CONTENT;
      break;

    case STATUS_CACHE_REQ_BAD_REQUEST:
      cache_req_status = STATUS_CACHE_REQ_BAD_REQUEST;
      break;

    case STATUS_CACHE_REQ_NOT_FOUND:
      cache_req_status = STATUS_CACHE_REQ_NOT_FOUND;
      break;

    case STATUS_CACHE_REQ_INTERNAL_SERVER_ERROR:
      cache_req_status = STATUS_CACHE_REQ_INTERNAL_SERVER_ERROR;
      break;

    default:
      break;
    }

    return cache_req_status;
  }

  int32_t CacheManager::_req_from_backend(StreamId_Ext stream_id, int32_t request_state, int32_t seq)
  {
    if (_module_type == MODULE_TYPE_UPLOADER)
    {
      return 0;
    }

    _init_cache_request_header();
    _temp_cache_request_header.stream_id = stream_id;
    _temp_cache_request_header.type = request_state;
    _temp_cache_request_header.seq = seq;

    //backend_start_stream_v3(stream_id, _temp_cache_request_header);

    return 0;
  }

  int32_t CacheManager::_req_from_backend_rtp(StreamId_Ext stream_id, int32_t request_state)
  {
    backend_start_stream_rtp(stream_id);
    return 0;
  }

  int32_t CacheManager::_req_stop_from_backend(StreamId_Ext stream_id, int32_t request_state, int32_t seq)
  {
    if (_module_type == MODULE_TYPE_UPLOADER)
    {
      return 0;
    }

    if (request_state == CACHE_REQ_LIVE_RTP)
    {
      _req_stop_from_backend_rtp(stream_id);
      return 0;
    }

    _init_cache_request_header();
    _temp_cache_request_header.stream_id = stream_id;
    _temp_cache_request_header.type = request_state;
    _temp_cache_request_header.seq = seq;

    //backend_stop_stream_v3(stream_id, _temp_cache_request_header);
    return 0;
  }

  int32_t CacheManager::_req_stop_from_backend(uint32_t stream_id)
  {
    if (_module_type == MODULE_TYPE_UPLOADER)
    {
      return 0;
    }

    //backend_stop_stream_v2(stream_id);
    return 0;
  }

  int32_t CacheManager::_req_stop_from_backend_rtp(StreamId_Ext stream_id)
  {
    backend_stop_stream_rtp(stream_id);
    return 0;
  }

  int32_t CacheManager::_req_stop_from_backend(StreamId_Ext stream_id)
  {
    if (_module_type == MODULE_TYPE_UPLOADER)
    {
      return 0;
    }

    //backend_stop_stream_v3(stream_id);
    return 0;
  }

  uint8_t CacheManager::_init_cache_request_header()
  {
    memset(&_temp_cache_request_header, 0, sizeof(cache_request_header));
    _temp_cache_request_header.version = CACHE_REQUEST_VERSION_2;
    return CACHE_REQUEST_VERSION_2;
  }

  uint8_t CacheManager::_init_cache_response_header()
  {
    memset(&_temp_cache_response_header, 0, sizeof(cache_response_header));
    _temp_cache_response_header.version = CACHE_RESPONSE_VERSION_2;
    return CACHE_RESPONSE_VERSION_2;
  }

  void CacheManager::start_timer()
  {
    struct timeval tv;
    levtimer_set(&_ev_timer, cache_timer_service, (void *)this);
    tv.tv_sec = _config->live_push_timeout_sec;
    tv.tv_usec = 0;
    event_base_set(_main_base, &_ev_timer);
    levtimer_add(&_ev_timer, &tv);

    _time_service_active = true;
  }

  void CacheManager::stop_timer()
  {
    if (_time_service_active)
    {
      levtimer_del(&_ev_timer);
      _time_service_active = false;
    }
  }

  static vector<StreamId_Ext> temp_remove_block_vec;

  void CacheManager::_check_stream_store_timeout()
  {
    if (_module_type == MODULE_TYPE_UPLOADER)
    {
      return;
    }

    time_t now = time(NULL);

    temp_remove_block_vec.clear();

    StreamStoreMap_t::iterator hash_map_it;

    for (hash_map_it = _stream_store_map.begin();
      hash_map_it != _stream_store_map.end(); hash_map_it++)
    {
      StreamStore* stream_store = (*hash_map_it).second;

      if (stream_store != NULL)
      {
        // we will clear this stream in forward and player if there is long time no output data.

        time_t last_req_time = stream_store->get_req_active_time();
        if (_config->live_req_timeout_sec > 0)
        {
          if (_module_type != MODULE_TYPE_UPLOADER && now - last_req_time >= _config->live_req_timeout_sec)
          {
            INF("remove stream: %s from cache manager because long time no request.",
              stream_store->stream_id.unparse().c_str());
            temp_remove_block_vec.push_back(stream_store->stream_id);
            continue;
          }
        }

        // we will require this stream again in forward and player if there is long time no input data.
        // 0. flv miniblock
        if (stream_store->flv_miniblock_cache != NULL)
        {
          time_t last_push_time = stream_store->flv_miniblock_cache->get_push_active_time();

          if (_module_type != MODULE_TYPE_UPLOADER
            && last_push_time != 0
            && now - last_push_time >= _config->live_push_timeout_sec)
          {
            WRN("flv_miniblock_cache long time no data for stream: %s, from: %ld, now: %ld, request from backend again.",
              stream_store->stream_id.unparse().c_str(), last_push_time, now);
            _req_from_backend((*hash_map_it).first, CACHE_REQ_LIVE_FLV_LATEST_MINIBLOCK);
          }
        }

        // 4. rtp media cache
        if (stream_store->rtp_media_cache != NULL)
        {
          time_t last_push_time = stream_store->rtp_media_cache->get_push_active_time();

          if (_module_type != MODULE_TYPE_UPLOADER
            && last_push_time != 0
            && now - last_push_time >= _config->live_push_timeout_sec)
          {
            WRN("rtp_media_cache long time no data for stream: %s, from: %ld, now: %ld, request from backend again.",
              stream_store->stream_id.unparse().c_str(), last_push_time, now);
            _req_from_backend((*hash_map_it).first, CACHE_REQ_LIVE_SDP);
            _req_from_backend_rtp((*hash_map_it).first, CACHE_REQ_LIVE_RTP);
          }
        }

      }
    }

    int num = temp_remove_block_vec.size();

    for (int i = 0; i < num; i++)
    {
      destroy_stream(temp_remove_block_vec[i]);
    }

    temp_remove_block_vec.clear();
  }

  void CacheManager::_adjust_flv_miniblock_cache_size()
  {
    StreamStoreMap_t::iterator store_it = _stream_store_map.begin();
    for (; store_it != _stream_store_map.end(); store_it++)
    {
      StreamStore* stream_store = store_it->second;
      FLVMiniBlockCircularCache* cache = stream_store->flv_miniblock_cache;
      if (cache == NULL)
      {
        continue;
      }

      int size = cache->size();
      while (cache->size() > 0)
      {
        int32_t status;
        BaseBlock* back_block = cache->back(status);
        BaseBlock* front_block = cache->front(status);
        int32_t max_duration = _config->flv_block_live_cache->buffer_duration_sec * 1000;
        int32_t max_size = _config->flv_block_live_cache->buffer_duration_sec * 100;

        if (((int32_t)back_block->get_start_ts() - (int32_t)front_block->get_start_ts() > max_duration)
          || ((int32_t)back_block->get_seq() - (int32_t)front_block->get_seq() > max_size))
        {
          TRC("flv_miniblock_cache pop FLVBlock, stream: %s, seq: %d",
            front_block->get_stream_id().unparse().c_str(), front_block->get_seq());
          cache->pop_front(true);
        }
        else
        {
          break;
        }
      }

      DBG("_adjust_flv_miniblock_cache_size, streamid: %s, before: %d, after %d",
        cache->stream_id().unparse().c_str(), size, cache->size());
    }
  }

}
