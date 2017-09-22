#include <sys/socket.h>
#include <stdlib.h>
#include <malloc.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>

#include "common/proto_define.h"
#include <common/proto.h>

#include "util/util.h"

#include "define.h"
#include "streamid.h"

#include <set>
#include <vector>
#include <algorithm>
#include "portal.h"
#include <appframe/singleton.hpp>
#include "whitelist_manager.h"

using namespace std;

Portal* Portal::instance = NULL;
static ModTrackerWhitelistClient* g_ModTrackerWhitelistClient = NULL;

Portal::Portal(bool enable_uploader, bool enable_player)
: _rkeepalive(NULL),
_fkeepalive(NULL),
_stream_cnt(0),
_streamids(NULL),
_enable_uploader(enable_uploader),
_enable_player(enable_player),
_info_stream_beats_cnt(0),
_player_keepalive_cnt(0),
_uploader_keepalive_cnt(0)
{
  _streamids = new uint32_t[MAX_STREAM_CNT];
  memset(_streamids, 0, sizeof(uint32_t)* MAX_STREAM_CNT);
  instance = this;
}

Portal::~Portal()
{
  if (_streamids != NULL)
  {
    delete[] _streamids;
    _streamids = NULL;
  }

  if (_rkeepalive != NULL)
  {
    delete[] _rkeepalive;
  }

  if (_fkeepalive != NULL)
  {
    delete[] _fkeepalive;
  }
}

void Portal::destroy()
{
  if (instance != NULL)
  {
    delete instance;
    instance = NULL;
  }
}

int Portal::init()
{
  _rkeepalive = new char[sizeof(r2p_keepalive)+MAX_STREAM_CNT * sizeof(receiver_stream_status)];
  _fkeepalive = new char[sizeof(f2p_keepalive)+MAX_STREAM_CNT * sizeof(forward_stream_status)];

  g_ModTrackerWhitelistClient = new ModTrackerWhitelistClient(this);
  ModTracker::get_inst().set_default_inbound_client(g_ModTrackerWhitelistClient);

  return 0;
}

Portal* Portal::get_instance()
{
  assert(Portal::instance != NULL);
  return Portal::instance;
}

void Portal::reset()
{
  TRC("portal_reset");
}

void Portal::inf_stream(stream_info* info, uint32_t cnt)
{
  set<uint32_t> temp_set;

  for (uint32_t i = 0; i < cnt; i++)
  {
    temp_set.insert(info[i].streamid);
  }

  vector<uint32_t> removal_vec;

  for (StreamMap_t::iterator it = _stream_map.begin(); it != _stream_map.end(); it++)
  {
    uint32_t stream_id = it->first;
    if (temp_set.find(stream_id) == temp_set.end())
    {
      removal_vec.push_back(stream_id);
    }
  }

  WhiteListMap &white_list = *SINGLETON(WhitelistManager)->get_white_list();
  timeval start_ts;
  gettimeofday(&start_ts, NULL);
  for (WhiteListMap::iterator it = white_list.begin(); it != white_list.end(); it++)
  {
    WhiteListItem& item = it->second;
    TRC("whitelist map has stream %s %d startsecond:%ld", item.streamid.c_str(), item.streamid.get_32bit_stream_id(), item.stream_start_ts.tv_sec);
    if (
      (temp_set.find(item.streamid.get_32bit_stream_id()) == temp_set.end())
      && (item.streamid.get_32bit_stream_id() != 0)
      && (start_ts.tv_sec - item.stream_start_ts.tv_sec > 30))
    {
      WRN("%s %d in fake whitelist has 30 seconds, but not in real whitelist so reject it upload", item.streamid.c_str(), item.streamid.get_32bit_stream_id());
      _destroy_stream_handler(item.streamid.get_32bit_stream_id());
      //INF("%s %d not in whitelist and stop success", item.streamid.c_str(), item.streamid.get_32bit_stream_id());
    }
  }

  for (uint32_t i = 0; i < removal_vec.size(); i++)
  {
    uint32_t stream_id = removal_vec[i];
    _stream_map.erase(stream_id);
    _destroy_stream_handler(stream_id);
    INF("portal close stream. streamid = %u", stream_id);
  }

  for (uint32_t i = 0; i < cnt; i++)
  {
    uint32_t stream_id = info[i].streamid;
    _create_stream_handler(info[i]);
    _stream_map[stream_id] = info[i];


    DBG("Portal::inf_stream: streamid: %032x", stream_id);
  }
}

void Portal::inf_stream(uint32_t * ids, uint32_t cnt)
{
  set<uint32_t> temp_set;

  for (uint32_t i = 0; i < cnt; i++)
  {
    temp_set.insert(ids[i]);
  }

  vector<uint32_t> removal_vec;

  for (StreamMap_t::iterator it = _stream_map.begin(); it != _stream_map.end(); it++)
  {
    uint32_t stream_id = it->first;
    if (temp_set.find(stream_id) == temp_set.end())
    {
      removal_vec.push_back(stream_id);
    }
  }

  for (uint32_t i = 0; i < removal_vec.size(); i++)
  {
    uint32_t stream_id = removal_vec[i];
    _stream_map.erase(stream_id);
    _destroy_stream_handler(stream_id);
    INF("portal close stream. streamid = %u", stream_id);
  }

  timeval start_ts;
  gettimeofday(&start_ts, NULL);

  for (uint32_t i = 0; i < cnt; i++)
  {
    uint32_t stream_id = ids[i];
    if (_stream_map.find(stream_id) == _stream_map.end())
    {
      stream_info info;

      info.streamid = stream_id;
      info.start_time.tv_sec = 0;
      info.start_time.tv_usec = 0;

      DBG("Portal::inf_stream: streamid: %032x", stream_id);

      _create_stream_handler(info);
      _stream_map[stream_id] = info;
    }
  }
}

int Portal::parse_cmd(buffer *rb)
{
  int ret = 0;
  int rtn = 0;

  if (buffer_data_len(rb) >= sizeof(proto_header))
  {
    proto_header h;

    if (0 != decode_header(rb, &h))
    {
      WRN("portal decode header failed. reseting...");
      reset();
      return 0;
    }

    if (buffer_data_len(rb) < h.size)
      return 0;

    switch (h.cmd)
    {
    case CMD_P2F_INF_STREAM:
    {
      rtn = decode_p2f_inf_stream(rb);
      if (rtn != 0)
      {
      WRN("decode p2f_info_stream failed. reseting...");
      reset();
      return 0;
      }
      DBG("Portal::parse_cmd got CMD_P2F_INF_STREAM");

      p2f_inf_stream *body =
      (p2f_inf_stream *)(buffer_data_ptr(rb) + sizeof(proto_header));

      if (body->cnt > MAX_STREAM_CNT)
      {
      WRN("portal inf stream cnt too large. cnt = %hu, max = %d", body->cnt, MAX_STREAM_CNT);
      }

      uint32_t count = min(MAX_STREAM_CNT, (int)body->cnt);
      inf_stream(body->streamids, count);
      DBG("portal inf stream. cnt = %hu", count);

      _info_stream_beats_cnt++;
      if (_info_stream_beats_cnt % 100 == 0)
      {
      INF("portal inf stream for 100 times. cnt = %hu", body->cnt);
      }
      break;
    }
    case CMD_P2F_START_STREAM:
    {
      rtn = decode_p2f_start_stream(rb);
      if (rtn != 0)
      {
        WRN("decode p2f_start_stream failed. reseting...");
        reset();
        return 0;
      }
      DBG("Portal::parse_cmd got CMD_P2F_START_STREAM");

      p2f_start_stream *body =
        (p2f_start_stream *)(buffer_data_ptr(rb) +
        sizeof(proto_header));

      stream_info info;
      info.streamid = body->streamid;

      info.start_time.tv_sec = 0;
      info.start_time.tv_usec = 0;

      _stream_map[info.streamid] = info;
      _create_stream_handler(info);
      INF("portal start stream. streamid = %u", body->streamid);
      break;
    }
    case CMD_P2F_CLOSE_STREAM:
    {
      rtn = decode_p2f_close_stream(rb);
      if (rtn != 0)
      {
        WRN("decode p2f_close_stream failed. reseting...");
        reset();
        return 0;
      }
      DBG("Portal::parse_cmd got CMD_P2F_CLOSE_STREAM");

      p2f_close_stream *body =
        (p2f_close_stream *)(buffer_data_ptr(rb) +
        sizeof(proto_header));

      _stream_map.erase(body->streamid);
      _destroy_stream_handler(body->streamid);
      INF("portal close stream. streamid = %032x", body->streamid);
      break;
    }
    case CMD_P2F_INF_STREAM_V2:
    {
      int ret = decode_p2f_inf_stream_v2(rb);
      if (ret)
      {
        if (ret == -3)
        {
          return 0;
        }
        else
        {
          WRN("decode p2f_inf_stream_v2 failed. reseting...");
          reset();
          return 0;
        }
      }

      DBG("Portal::parse_cmd got CMD_P2F_INF_STREAM_V2");
      p2f_inf_stream_v2 *body =
        (p2f_inf_stream_v2 *)(buffer_data_ptr(rb) + sizeof(proto_header));

      if (body->cnt > MAX_STREAM_CNT)
      {
        WRN("portal inf stream cnt too large. cnt = %hu, max = %d",
          body->cnt, MAX_STREAM_CNT);
      }

      uint32_t count = min(MAX_STREAM_CNT, (int)body->cnt);
      inf_stream(body->stream_infos, count);
      DBG("portal inf stream. cnt = %hu", count);

      _info_stream_beats_cnt++;
      if (_info_stream_beats_cnt % 100 == 0)
      {
        INF("portal inf stream for 100 times. cnt = %hu", body->cnt);
      }
      break;
    }
    case CMD_P2F_START_STREAM_V2:
    {
      rtn = decode_p2f_start_stream_v2(rb);
      if (rtn != 0)
      {
        WRN("decode p2f_start_stream_v2 failed. reseting...");
        reset();
        return 0;
      }

      DBG("Portal::parse_cmd got CMD_P2F_START_STREAM_V2");
      p2f_start_stream_v2 *body =
        (p2f_start_stream_v2 *)(buffer_data_ptr(rb) +
        sizeof(proto_header));

      stream_info info;
      info.streamid = body->streamid;
      info.start_time.tv_sec = body->start_time.tv_sec;
      info.start_time.tv_usec = body->start_time.tv_usec;

      _stream_map[info.streamid] = info;
      _create_stream_handler(info);
      INF("portal start stream. streamid = %032x", body->streamid);
      break;
    }
    case CMD_P2F_KEEPALIVE_ACK:
      DBG("Portal::parse_cmd got CMD_P2F_KEEPALIVE_ACK");
      break;
    case CMD_P2R_KEEPALIVE_ACK:
      DBG("Portal::parse_cmd got CMD_P2R_KEEPALIVE_ACK");
      break;
    default:
      ERR("unexpected portal cmd = %hu, reseting...", h.cmd);
      break;
    }
    ret += h.size;
  }

  return ret;
}

void Portal::keepalive()
{
  TRC("keepalive to portal");

  proto_header ph = { 0, 0, CMD_F2P_WHITE_LIST_REQ, 0 };
  proto_wrapper pw = { &ph, NULL };
  bool ret = ModTracker::get_inst().send_request(&pw, g_ModTrackerWhitelistClient);
  if (ret)
  {
    DBG("Portal::keepalive: send success");
  }
  else
  {
    DBG("Portal::keepalive: send fail");
  }

  if (_enable_player)
  {
    proto_header ph_f2p = { 0, 0, CMD_F2P_KEEPALIVE, 0 };
    f2p_keepalive *f2p_ka = (f2p_keepalive *)_fkeepalive;
    f2p_ka = _f2p_keepalive_handler(f2p_ka);
    proto_wrapper pw_f2p = { &ph_f2p, f2p_ka };
    bool ret_f2p = ModTracker::get_inst().send_request(&pw_f2p,
      g_ModTrackerWhitelistClient);
    if (ret_f2p)
    {
      DBG("Portal::keepalive: send success");
    }
    else
    {
      DBG("Portal::keepalive: send fail");
    }

    DBG("Portal::keepalive: forward keepalive to tracker. stream_cnt = %u, \
                        out_speed = %u", f2p_ka->stream_cnt, f2p_ka->out_speed);
    _player_keepalive_cnt++;
    if (_player_keepalive_cnt % 100 == 0)
    {
      INF("Portal::keepalive: forward keepalive to portal for 100 times.  \
                          stream_cnt = %u, out_speed = %u", f2p_ka->stream_cnt, f2p_ka->out_speed);
    }
  }

  if (_enable_uploader)
  {
    r2p_keepalive *rka = (r2p_keepalive *)_rkeepalive;
    rka = _r2p_keepalive_handler(rka);
    proto_header ph_r2p = { 0, 0, CMD_R2P_KEEPALIVE, 0 };
    proto_wrapper pw_r2p = { &ph_r2p, rka };
    bool ret_r2p = ModTracker::get_inst().send_request(&pw_r2p,
      g_ModTrackerWhitelistClient);
    if (ret_r2p)
    {
      DBG("Portal::keepalive: send success");
    }
    else
    {
      DBG("Portal::keepalive: send fail");
    }

    /*if (0 != encode_r2p_keepalive(rka, _write_buf))
      ERR("encode_r2p_keepalive failed.");*/
    DBG("Portal::keepalive: receiver keepalive to portal. stream_cnt = %u, \
                    in_speed = %u, out_speed = %u", \
                    rka->stream_cnt, rka->inbound_speed, rka->outbound_speed);
    _uploader_keepalive_cnt++;
    if (_uploader_keepalive_cnt % 100 == 0)
    {
      INF("receiver keepalive to portal for 100 times. stream_cnt = %u, \
                              in_speed = %u, out_speed = %u", \
                              rka->stream_cnt, rka->inbound_speed, rka->outbound_speed);
    }
  }
}

void Portal::register_create_stream(void(*handler)(stream_info&))
{
  _create_stream_handler = handler;
}

void Portal::register_destroy_stream(void(*handler)(uint32_t))
{
  _destroy_stream_handler = handler;
}

void Portal::register_f2p_keepalive_handler(f2p_keepalive* (*handler)(f2p_keepalive*))
{
  _f2p_keepalive_handler = handler;
}

void Portal::register_r2p_keepalive_handler(r2p_keepalive* (*handler)(r2p_keepalive*))
{
  _r2p_keepalive_handler = handler;
}

ModTrackerWhitelistClient::ModTrackerWhitelistClient(Portal *portal) : ModTrackerClientBase()
{
  DBG("ModTrackerWhitelistClient(), ptr=%lu", uint64_t(this));
  _portal = portal;
}

ModTrackerWhitelistClient::~ModTrackerWhitelistClient()
{
  DBG("~ModTrackerWhitelistClient(), ptr=%lu", uint64_t(this));
}

int ModTrackerWhitelistClient::encode(const void* data, buffer* wb)
{
  if (NULL == data)
  {
    WRN("ModTrackerWhitelistClient::encode:data is null!");
    return -1;
  }

  const proto_wrapper* packet = reinterpret_cast<const proto_wrapper*>(data);
  if (NULL == packet->header)
  {
    WRN("ModTrackerWhitelistClient::encode: packet->header is null!");
    return -1;
  }

  proto_t cmd = packet->header->cmd;
  if (cmd <= 0)
  {
    WRN("ModTrackerWhitelistClient::encode:cmd is invalid!");
    return -1;
  }

  int status = -1;
  int size = 0;

  switch (cmd)
  {
  case CMD_F2P_WHITE_LIST_REQ:
  {
    size = sizeof(proto_header);
    status = encode_header_v3(wb, CMD_F2P_WHITE_LIST_REQ, size);
    break;
  }
  case CMD_F2P_KEEPALIVE:
  {
    f2p_keepalive* keepalive = (f2p_keepalive*)packet->payload;
    uint32_t stream_count = keepalive->stream_cnt;
    size = sizeof(proto_header)+sizeof(f2p_keepalive)+sizeof(forward_stream_status)* stream_count;
    status = encode_f2p_keepalive((f2p_keepalive*)packet->payload, wb);
    if (status != 0)
    {
      ERR("ModTrackerWhitelistClient::encode: encode failed");
    }
    break;
  }
  case CMD_R2P_KEEPALIVE:
  {
    r2p_keepalive* keepalive = (r2p_keepalive*)packet->payload;
    uint32_t stream_count = keepalive->stream_cnt;
    size = sizeof(proto_header)+sizeof(r2p_keepalive)+sizeof(receiver_stream_status)* stream_count;
    status = encode_r2p_keepalive((r2p_keepalive*)packet->payload, wb);
    if (status != 0)
    {
      ERR("ModTrackerWhitelistClient::encode: encode failed");
    }
    break;
  }
  default:
    WRN("ModTrackerWhitelistClient::encode:invalid cmd!");
    return -1;
  }

  DBG("ModTrackerWhitelistClient::encode: cmd=%d, status=%d", int(cmd), int(status));

  if (status == -1)
  {
    return -1;
  }
  else
  {
    return size;
  }
}

int ModTrackerWhitelistClient::decode(buffer* rb)
{
  if (NULL == rb)
  {
    WRN("ModTrackerWhitelistClient::decode:rb is null!");
    return -1;
  }

  int ret = 0;

  proto_header header;
  if (decode_header(rb, &header) == -1)
  {
    WRN("ModTrackerWhitelistClient::decode:decode_header error!");
    return -1;
  }

  DBG("ModTrackerWhitelistClient::decode: cmd=%d", int(header.cmd));
  ret += _portal->parse_cmd(rb);

  return ret;
}
