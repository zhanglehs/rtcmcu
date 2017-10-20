/**
 * @file cache_manager_state.cpp
 * @brief some function to get the running state and statistics of cache manager. \n
 * @author songshenyi
 * <pre><b>copyright: Youku</b></pre>
 * <pre><b>email: </b>songshenyi@youku.com</pre>
 * <pre><b>company: </b>http://www.youku.com</pre>
 * <pre><b>All rights reserved.</b></pre>
 * @date 2014/12/04
 * @see  cache_manager.h
 */

#include "util/util.h"
#include "utils/buffer.hpp"
#include "util/log.h"
#include "network/base_http_server.h"
#include "fragment/fragment_generator.h"
#include <deque>

#include "cache_manager.h"
//#include <appframe/singleton.hpp>
#include <json.h>

using namespace std;
using namespace fragment;

namespace {
  //void cache_manager_state(char* query, char* param, json_object* rsp, void* arg) {
  //  media_manager::FlvCacheManagerStatistic* pThis = (media_manager::FlvCacheManagerStatistic*)arg;
  //  pThis->http_state(query, param, rsp);
  //}

  //void cache_manager_state_handle(HTTPConnection* conn, void* arg) {
  //}

  //void cache_manager_state_check_handle(HTTPConnection* conn, void* arg) {
  //  json_object* rsp = json_object_new_object();
  //  HTTPRequest* request = conn->request;
  //  cache_manager_state((char *)(request->uri.c_str()), NULL, rsp, arg);
  //  conn->writeResponse(rsp);
  //  json_object_put(rsp);
  //  conn->stop();
  //}
}

namespace media_manager {

  void FlvCacheManagerStatistic::http_state(char* query, char* param, json_object* rsp)
  {
    char buf[128];
    util_query_str_get(query, strlen(query), "state", buf, 128);

    string state(buf);

    util_query_str_get(query, strlen(query), "stream", buf, 128);

    HttpHandlerMap_t::iterator it = _http_handler_map.find(state);

    if (it == _http_handler_map.end())
    {
      ERR("path not valid. path = %s", query);
      json_object* error = json_object_new_string("path not valid.");
      json_object_object_add(rsp, "error", error);
      return;
    }
    else
    {
      HttpHandler_t handler = it->second;
      (this->*handler)(query, buf, rsp);
    }
  }


  //void FlvCacheManagerStatistic::set_http_server(http::HTTPServer *server)
  //{
  //  _init_http_server();
  //  server->add_handle("/cache_manager",
  //    cache_manager_state_handle,
  //    this,
  //    cache_manager_state_check_handle,
  //    this,
  //    NULL,
  //    NULL);
  //}

  void FlvCacheManagerStatistic::_init_http_server()
  {
    _add_http_handler("cache", NULL, &FlvCacheManagerStatistic::_state);
    _add_http_handler("stream_list", NULL, &FlvCacheManagerStatistic::_stream_list);
    _add_http_handler("store", "stream", &FlvCacheManagerStatistic::_store_state);
    _add_http_handler("whitelist_fake", NULL, &FlvCacheManagerStatistic::_whitelist_fake);
  }

  void FlvCacheManagerStatistic::_add_http_handler(const char* query, const char* param, HttpHandler_t handler)
  {
    _http_handler_map[string(query)] = handler;
  }

  void FlvCacheManagerStatistic::_stream_list(char* query, char* param, json_object* rsp)
  {
    //json_object* store_map_size = json_object_new_int(_stream_store_map.size());
    //json_object_object_add(rsp, "stream_store_map_size", store_map_size);

    //json_object* store_map = json_object_new_array();
    //StreamStoreMap_t::iterator store_it = _stream_store_map.begin();
    //for (; store_it != _stream_store_map.end(); store_it++)
    //{
    //  StreamId_Ext stream_id = store_it->first;
    //  json_object* id = json_object_new_string(stream_id.unparse().c_str());
    //  json_object_array_add(store_map, id);
    //}
    //json_object_object_add(rsp, "store_map", store_map);
  }

  void FlvCacheManagerStatistic::_state(char* query, char* param, json_object* rsp)
  {
    //json_object* store_map_size = json_object_new_int(_stream_store_map.size());
    //json_object_object_add(rsp, "stream_store_map_size", store_map_size);

    //json_object* store_map = json_object_new_array();
    //StreamStoreMap_t::iterator store_it = _stream_store_map.begin();
    //for (; store_it != _stream_store_map.end(); store_it++)
    //{
    //  json_object* obj = json_object_new_object();
    //  StreamStore* stream_store = store_it->second;

    //  StreamId_Ext stream_id = store_it->first;
    //  json_object* id = json_object_new_string(stream_id.unparse().c_str());
    //  json_object_object_add(obj, "stream_id", id);

    //  uint8_t state = stream_store->state;
    //  json_object* state_obj = json_object_new_int(state);
    //  json_object_object_add(obj, "state", state_obj);

    //  if (stream_store->flv_miniblock_generator == NULL)
    //  {
    //    json_object* flv_miniblock_generator = json_object_new_string("null");
    //    json_object_object_add(obj, "flv_miniblock_generator", flv_miniblock_generator);
    //  }
    //  else
    //  {
    //    //FLVMiniBlockGenerator* block_generator = stream_store->flv_miniblock_generator;
    //    //json_object* start_timestamp = json_object_new_int64(block_generator->_last_cut_timestamp);
    //    //json_object* flv_miniblock_generator = json_object_new_object();
    //    //json_object_object_add(flv_miniblock_generator, "_last_cut_timestamp", start_timestamp);
    //    //json_object_object_add(obj, "flv_miniblock_generator", flv_miniblock_generator);
    //  }

    //  if (stream_store->flv_miniblock_cache == NULL)
    //  {
    //    json_object* flv_miniblock_cache = json_object_new_string("null");
    //    json_object_object_add(obj, "flv_miniblock_cache", flv_miniblock_cache);
    //  }
    //  else
    //  {
    //    FLVMiniBlockCircularCache* cache = stream_store->flv_miniblock_cache;
    //    json_object* cache_size = json_object_new_int64(cache->size());
    //    json_object* cache_json = json_object_new_object();
    //    json_object_object_add(cache_json, "size", cache_size);
    //    json_object_object_add(obj, "flv_miniblock_cache", cache_json);
    //  }

    //  json_object* last_push_time = json_object_new_int64(stream_store->get_push_active_time());
    //  json_object* last_req_time = json_object_new_int64(stream_store->get_req_active_time());

    //  json_object_object_add(obj, "last_push_time", last_push_time);
    //  json_object_object_add(obj, "last_req_time", last_req_time);

    //  json_object_array_add(store_map, obj);
    //}
    //json_object_object_add(rsp, "store_map", store_map);
  }

  void FlvCacheManagerStatistic::_store_state(char* query, char* param, json_object* rsp)
  {
    //if (param == NULL || strlen(param) == 0)
    //{
    //  json_object* error = json_object_new_string("no such stream");
    //  json_object_object_add(rsp, "error", error);
    //  return;
    //}

    //StreamId_Ext stream_id;
    //stream_id.parse(param);
    //StreamStoreMap_t::iterator it = _stream_store_map.find(stream_id);
    //if (it == _stream_store_map.end())
    //{
    //  json_object* error = json_object_new_string("no such stream");
    //  json_object_object_add(rsp, "error", error);
    //  return;
    //}

    //StreamStore* stream_store = it->second;

    //json_object* id = json_object_new_string(stream_id.unparse().c_str());
    //json_object_object_add(rsp, "stream_id", id);

    //if (stream_store->flv_miniblock_generator == NULL)
    //{
    //  json_object* flv_miniblock_generator = json_object_new_string("null");
    //  json_object_object_add(rsp, "flv_miniblock_generator", flv_miniblock_generator);
    //}
    //else
    //{
    //  json_object* generator_json = json_object_new_object();
    //  _flv_miniblock_generator_state(stream_store->flv_miniblock_generator, generator_json);
    //  json_object_object_add(rsp, "flv_miniblock_generator", generator_json);
    //}

    //if (stream_store->flv_miniblock_cache == NULL)
    //{
    //  json_object* flv_miniblock_cache = json_object_new_string("null");
    //  json_object_object_add(rsp, "flv_miniblock_cache", flv_miniblock_cache);
    //}
    //else
    //{
    //  FLVMiniBlockCircularCache* cache = stream_store->flv_miniblock_cache;
    //  json_object* cache_json = json_object_new_object();
    //  _flv_live_miniblock_cache_state(cache, cache_json);
    //  json_object_object_add(rsp, "flv_miniblock_cache", cache_json);
    //}
  }

  void FlvCacheManagerStatistic::_whitelist_fake(char* query, char* param, json_object* rsp)
  {
    //WhiteListMap &white_list = *SINGLETON(WhitelistManager)->get_white_list();
    //int nWhitelistSize = white_list.size();
    //json_object* store_map_size = json_object_new_int(nWhitelistSize);
    //json_object_object_add(rsp, "fake_whitelist_size", store_map_size);

    //json_object* store_map = json_object_new_array();
    //WhiteListMap::iterator store_it = white_list.begin();
    //for (; store_it != white_list.end(); store_it++)
    //{
    //  json_object* obj = json_object_new_object();

    //  StreamId_Ext stream_id = store_it->first;
    //  json_object* id = json_object_new_string(stream_id.unparse().c_str());
    //  json_object_object_add(obj, "stream_id", id);

    //  WhiteListItem whitelistItem = store_it->second;
    //  long second = whitelistItem.stream_start_ts.tv_sec;
    //  json_object* second_int = json_object_new_int64(second);
    //  json_object_object_add(obj, "start_second_int64", second_int);

    //  time_t tick = second;
    //  struct tm tm;
    //  char s[100];
    //  tm = *localtime(&tick);
    //  strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", &tm);
    //  //printf("gaoliwen %d: %s\n", (int)tick, s);
    //  json_object* second_str = json_object_new_string(s);
    //  json_object_object_add(obj, "start_second_str", second_str);

    //  json_object_array_add(store_map, obj);
    //}
    //json_object_object_add(rsp, "fake_whitelist", store_map);
  }

  void FlvCacheManagerStatistic::_fragment_generator_state(fragment::FragmentGenerator* generator, json_object* rsp)
  {
    //deque<flv_tag*>::iterator it = generator->_temp_tag_store.begin();

    //uint32_t data_len = 0;
    //for (; it != generator->_temp_tag_store.end(); it++)
    //{
    //  flv_tag* tag = *it;
    //  data_len += flv_get_datasize(tag->datasize) + sizeof(flv_tag)+4;
    //}

    //json_object* buf_tag_store_size = json_object_new_int64(generator->_temp_tag_store.size());
    //json_object_object_add(rsp, "buf_tag_store_size", buf_tag_store_size);

    //json_object* buf_data_len = json_object_new_int64(data_len);
    //json_object_object_add(rsp, "buffer_data_len", buf_data_len);

    //json_object* main_type = json_object_new_int(generator->_main_type);
    //json_object_object_add(rsp, "main_type", main_type);

    //json_object* cut_by = json_object_new_int(generator->_cut_by);
    //json_object_object_add(rsp, "cut_by", cut_by);

    //json_object* keyframe_num = json_object_new_int(generator->_keyframe_num);
    //json_object_object_add(rsp, "keyframe_num", keyframe_num);

    //json_object* flv_header_len = json_object_new_int(generator->_flv_header_len);
    //json_object_object_add(rsp, "flv_header_len", flv_header_len);

    //json_object* flv_header_flag = json_object_new_int(generator->_flv_header_flag);
    //json_object_object_add(rsp, "flv_header_flag", flv_header_flag);

    //json_object* input_tag_num = json_object_new_int(generator->_input_tag_num);
    //json_object_object_add(rsp, "input_tag_num", input_tag_num);

    //json_object* last_seq = json_object_new_int(generator->_last_seq);
    //json_object_object_add(rsp, "last_seq", last_seq);

    //json_object* last_key_seq = json_object_new_int(generator->_last_key_seq);
    //json_object_object_add(rsp, "last_key_seq", last_key_seq);
  }

  void FlvCacheManagerStatistic::_flv_miniblock_generator_state(fragment::FLVMiniBlockGenerator* generator, json_object* rsp)
  {
    if (generator == NULL)
    {
      json_object* flv_miniblock_generator = json_object_new_string("null");
      json_object_object_add(rsp, "flv_miniblock_generator", flv_miniblock_generator);
    }
    else
    {
      _fragment_generator_state(generator, rsp);
    }
  }

  void FlvCacheManagerStatistic::_live_cache_state(CircularCache* cache, json_object* rsp)
  {
    //json_object* block_store_size = json_object_new_int(cache->_block_store.size());
    //json_object_object_add(rsp, "block_store_size", block_store_size);

    //if (cache->_block_store.size() > 0)
    //{
    //  int size = cache->_block_store.size();

    //  json_object* first_block_seq = json_object_new_int(cache->_block_store[0]->get_seq());
    //  json_object_object_add(rsp, "first_block_seq", first_block_seq);

    //  json_object* first_block_ts = json_object_new_int(cache->_block_store[0]->get_start_ts());
    //  json_object_object_add(rsp, "first_block_ts", first_block_ts);

    //  json_object* last_block_seq = json_object_new_int(cache->_block_store[size - 1]->get_seq());
    //  json_object_object_add(rsp, "last_block_seq", last_block_seq);

    //  json_object* last_block_ts = json_object_new_int(cache->_block_store[size - 1]->get_start_ts());
    //  json_object_object_add(rsp, "last_block_ts", last_block_ts);
    //}
  }

  void FlvCacheManagerStatistic::_flv_live_miniblock_cache_state(FLVMiniBlockCircularCache* cache, json_object* rsp)
  {
    //_live_cache_state(cache, rsp);

    //json_object* flv_header_len = json_object_new_int(cache->_flv_header_len);
    //json_object_object_add(rsp, "flv_header_len", flv_header_len);

    //json_object* flv_header_flag = json_object_new_int(cache->_flv_header_flag);
    //json_object_object_add(rsp, "flv_header_flag", flv_header_flag);
  }

}
