/*
 *
 *
 */

#include "whitelist_manager.h"

#include <appframe/singleton.hpp>
#include <util/log.h>

#include "config.h"

#include "backend_new/module_backend.h"
#include "media_manager/cache_manager.h"
#include "player/module_player.h"
#include "stream_manager.h"
#include "target_player.h"
#include "target_backend.h"
#include "uploader/RtpTcpConnectionManager.h"
#include "stream_recorder.h"    // for stop_record_stream
#include "define.h" // for MAX_STREAM_CNT

using namespace  media_manager;

static config &_conf = *SINGLETON(config);

//#todo delete in develop_refactor
boolean tb_is_src_stream_v2(StreamId_Ext streamid)
{
  return SINGLETON(WhitelistManager)->tb_is_src_stream_v2(streamid);
}
int notify_need_stream(uint32_t streamid)
{
  return SINGLETON(WhitelistManager)->notify_need_stream(streamid);
}

void target_insert_list_seat(stream_info& info)
{
  SINGLETON(WhitelistManager)->add_stream(info);
}

void WhitelistManager::add_stream(stream_info& info)
{
  TRC("WhitelistManager::add_stream. streamid = %u", info.streamid);

  StreamId_Ext streamid_ext(info.streamid, 0);

  bool add = false;
  if (_white_list.find(streamid_ext) == _white_list.end())
  {
    _white_list[streamid_ext] = WhiteListItem(streamid_ext, info.start_time);
    INF("insert list seat success. streamid = %u, start_time, sec: %ld, usec: %ld",
      info.streamid, info.start_time.tv_sec, info.start_time.tv_usec);
    add = true;
  }
  else
  {
    if (info.start_time.tv_sec > 0 &&
      _white_list[streamid_ext].stream_start_ts.tv_sec == 0)
    {
      _white_list[streamid_ext].stream_start_ts = info.start_time;
      INF("insert list seat success. streamid = %u, start_time, sec: %ld, usec: %ld",
        info.streamid, info.start_time.tv_sec, info.start_time.tv_usec);

      add = true;
    }
  }

  if (add)
  {
    for (ObserverList::iterator i = _observer_list.begin();
      i != _observer_list.end(); i++)
    {
      OB_EV &ob_ev = *i;
      if (WHITE_LIST_EV_START == (ob_ev.event & WHITE_LIST_EV_START))
      {
        ob_ev.observer->update(streamid_ext, WHITE_LIST_EV_START);
      }
    }
  }
}

void target_close_list_seat(uint32_t streamid)
{
  SINGLETON(WhitelistManager)->del_stream(streamid);
}

void WhitelistManager::del_stream(uint32_t streamid)
{
  //TRC("WhitelistManager::del_stream. streamid = %u", streamid);

  //StreamId_Ext orignal_streamid(streamid, 0);
  //if (_white_list.find(orignal_streamid) == _white_list.end())
  //{
  //  WRN("streamid %s not in white list. do nothing.", orignal_streamid.unparse().c_str());
  //  return;
  //}

  //if (strlen(_conf.target_conf.record_dir) > 0)
  //{
  //  stop_record_stream(streamid);
  //}
  //TRC("begin manager destroy stream. streamid = %u", streamid);
  //stream_manager_destroy_stream(streamid);

  //WhiteListMap::iterator it;
  //vector<StreamId_Ext> removal_list;

  ////UploaderManager* uploader_manager = UploaderManager::get_inst();
  //for (it = _white_list.begin(); it != _white_list.end(); it++)
  //{
  //  if (it->second.streamid.get_32bit_stream_id() == streamid)
  //  {
  //    WhiteListItem& item = it->second;

  //    removal_list.push_back(item.streamid);
  //  }
  //}

  //CacheManager::get_cache_manager()->stop_stream(streamid);

  //for (int i = 0; i < (int)removal_list.size(); i++)
  //{
  //  _white_list.erase(removal_list[i]);
  //  INF("close list seat success. streamid = %s", removal_list[i].unparse().c_str());
  //  //}

  //  for (ObserverList::iterator oi = _observer_list.begin();
  //    oi != _observer_list.end(); oi++)
  //  {
  //    OB_EV &ob_ev = *oi;
  //    if (WHITE_LIST_EV_STOP == (ob_ev.event & WHITE_LIST_EV_STOP))
  //    {
  //      //ob_ev.observer->update(orignal_streamid, WHITE_LIST_EV_STOP);
  //      ob_ev.observer->update(removal_list[i], WHITE_LIST_EV_STOP);
  //    }
  //  }
  //}
}

void WhitelistManager::del_stream(const StreamId_Ext& streamid) {
  for (ObserverList::iterator it = _observer_list.begin();
    it != _observer_list.end(); it++) {
    OB_EV &ob_ev = *it;
    if (WHITE_LIST_EV_STOP == (ob_ev.event & WHITE_LIST_EV_STOP)) {
      ob_ev.observer->update(streamid, WHITE_LIST_EV_STOP);
    }
  }

  FlvCacheManager* uploader_cache_instance = FlvCacheManager::Instance();
  uploader_cache_instance->destroy_stream(streamid);
}

r2p_keepalive* build_r2p_keepalive(r2p_keepalive* rka)
{
  return SINGLETON(WhitelistManager)->build_r2p_keepalive(rka);
}
r2p_keepalive* WhitelistManager::build_r2p_keepalive(r2p_keepalive* rka)
{
  //if (!_conf.target_conf.enable_uploader)
  //  return rka;

  //inet_aton(_conf.uploader.listen_ip,
  //  (struct in_addr *) &rka->listen_uploader_addr.ip);
  //rka->listen_uploader_addr.ip = ntohl(rka->listen_uploader_addr.ip);
  //rka->listen_uploader_addr.port = _conf.uploader.listen_port;
  //rka->outbound_speed = rka->inbound_speed = 0;
  //rka->stream_cnt = 0;

  //receiver_stream_status *rss = NULL;

  //WhiteListMap::iterator it;

  //CacheManager* cache = CacheManager::get_cache_manager();

  //for (it = _white_list.begin(); it != _white_list.end(); it++)
  //{
  //  WhiteListItem& item = it->second;

  //  if (item.streamid != StreamId_Ext(item.streamid.get_32bit_stream_id(), 0))
  //  {
  //    continue;
  //  }

  //  int32_t status = 0;
  //  StreamStore* store = cache->get_stream_store(item.streamid, status);

  //  if (store == NULL)
  //    continue;

  //  rss = rka->streams + rka->stream_cnt;
  //  rss->streamid = item.streamid.get_32bit_stream_id();
  //  rss->forward_cnt = 0;

  //  rss->last_ts = store->get_last_push_relative_timestamp_ms();
  //  rss->block_seq = store->get_last_block_seq();

  //  rka->outbound_speed += DEFAULT_BPS * rss->forward_cnt / 8;
  //  rka->inbound_speed += DEFAULT_BPS / 8;
  //  rka->stream_cnt++;
  //  //DBG("build build_r2p_keepalive for stream: %s seq %d", item.streamid.unparse().c_str(), rss->block_seq);

  //  //       printf("build build_r2p_keepalive for stream: %s , seq %llu\n", item.streamid.unparse().c_str(), rss->block_seq);

  //  if (rka->stream_cnt >= MAX_STREAM_CNT)
  //    break;
  //}

  //DBG("build_r2p_keepalive rka->stream_cnt: %d", rka->stream_cnt);

  return rka;
}

f2p_keepalive* build_f2p_keepalive(f2p_keepalive* ka)
{
  return SINGLETON(WhitelistManager)->build_f2p_keepalive(ka);
}

f2p_keepalive* WhitelistManager::build_f2p_keepalive(f2p_keepalive* ka)
{
  inet_aton(_conf.player.player_listen_ip,
    (struct in_addr *) &ka->listen_player_addr.ip);
  ka->listen_player_addr.ip = ntohl(ka->listen_player_addr.ip);
  ka->listen_player_addr.port = _conf.player.player_listen_port;
  ka->out_speed = 0;
  ka->stream_cnt = 0;

  forward_stream_status *fss = NULL;

  for (WhiteListMap::iterator it = _white_list.begin(); it != _white_list.end(); it++)
  {
    WhiteListItem& item = it->second;

    if (!stream_manager_is_has_stream(item.streamid.get_32bit_stream_id()))
      continue;
    fss = ka->stream_status + ka->stream_cnt;
    fss->streamid = item.streamid.get_32bit_stream_id();
    fss->player_cnt = fss->forward_cnt = 0;
    fss->last_ts = -1;
    fss->last_block_seq = (uint64_t)-1;

    block_map *bm = (block_map *)stream_manager_get_last_block(item.streamid.get_32bit_stream_id(), NULL);

    if (NULL != bm)
    {
      fss->last_block_seq = bm->data.seq;
      fss->last_ts = bm->last_ts;
    }

    ka->out_speed +=
      DEFAULT_BPS * (fss->player_cnt + fss->forward_cnt) / 8;
    ka->stream_cnt++;
    if (ka->stream_cnt >= MAX_STREAM_CNT)
      break;
  }
  return ka;
}

WhiteListItem::WhiteListItem()
{
  stream_mng_created = FALSE;
  cache_manager_created = FALSE;
  is_idle = FALSE;
  idle_sec = 0;
  stream_start_ts.tv_sec = 0;
  stream_start_ts.tv_usec = 0;
  //player_ptr   = NULL;
  //uploader_ptr = NULL;
}

WhiteListItem::WhiteListItem(const StreamId_Ext& stream_id, const timeval start_ts)
{
  stream_mng_created = FALSE;
  cache_manager_created = FALSE;
  streamid = stream_id;
  is_idle = FALSE;
  idle_sec = 0;
  stream_start_ts = start_ts;
  //player_ptr   = NULL;
  //uploader_ptr = NULL;
}

WhiteListItem::WhiteListItem(const WhiteListItem& right)
{
  streamid = right.streamid;
  is_idle = right.is_idle;
  stream_start_ts = right.stream_start_ts;
  //player_ptr      = right.player_ptr;
  //uploader_ptr    = right.uploader_ptr;
  idle_sec = right.idle_sec;
  stream_mng_created = right.stream_mng_created;
  cache_manager_created = right.cache_manager_created;
}

bool WhitelistManager::in(const StreamId_Ext& id)
{
  return true;
  //StreamId_Ext original_id(id.get_32bit_stream_id(), 0);
  //if (_white_list.find(original_id) == _white_list.end())
  //{
  //  return false;
  //}
  //else
  //{
  //  return true;
  //}
}

WhiteListMap* WhitelistManager::get_white_list()
{
  return &_white_list;
}

boolean WhitelistManager::tb_is_src_stream_v2(StreamId_Ext streamid)
{
  if (_white_list.find(streamid) == _white_list.end())
  {
    return FALSE;
  }

  return FALSE;
}

int WhitelistManager::notify_need_stream(uint32_t streamid)
{
  StreamId_Ext streamid_ext(streamid, 0);
  if (_white_list.find(streamid_ext) == _white_list.end())
  {
    WRN("been notified with invalid streamid = %u", streamid);
    return -1;
  }

  int ret = -1;
  WhiteListItem& ls = _white_list[streamid_ext];
  ls.is_idle = FALSE;
  if (!ls.stream_mng_created) {
    ret = stream_manager_create_stream(streamid);
    if (0 != ret && -1 != ret) {
      ERR("stream_manager_create_stream failed. streamid = %u",
        streamid);
      return -1;
    }
    ls.stream_mng_created = TRUE;
    if (-1 == ret)
      DBG("stream already in stream manager. streamid = %u", streamid);
  }
  return 0;
}

void WhitelistManager::clear()
{
  TRC("delete _white_list");
  _white_list.clear();
}

int WhitelistManager::add_observer(WhitelistObserver *observer, WhitelistEvent interest_ev)
{
   _observer_list.push_front(OB_EV(observer, interest_ev));
  return 0;
}

int WhitelistManager::del_observer(WhitelistObserver *observer)
{
  ObserverList::iterator i = _observer_list.begin();
  for (; i != _observer_list.end(); i++)
  {
    WhitelistObserver *ob = (*i).observer;
    if ((void*)observer == (void*)ob)
    {
      _observer_list.erase(i);
      break;
    }
  }

  return 0;
}
