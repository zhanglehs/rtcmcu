#include <stddef.h>
#include <stdlib.h>
#include "common/proto_define.h"
#include "util/util.h"
#include "util/log.h"
#include "cache_manager.h"
#include "module_tracker.h"

#include "forward_server.h"
#include "utils/buffer.hpp"
#include "player/module_player.h"

#include "common/protobuf/f2t_register_req.pb.h"
#include "uploader_config.h"
#include "../network/base_http_server.h"

#define F2T_PB_BUFFER_SIZE 1024
struct F2TPBBuffer
{
  char buff[F2T_PB_BUFFER_SIZE];
  size_t size;
};

extern "C"
{
  boolean tb_is_src_stream_v2(StreamId_Ext streamid);
  int get_forward_level_v2(StreamId_Ext streamid, uint16_t *level);
}

using namespace lcdn;
using namespace base;
using namespace net;
using namespace interlive;
using namespace interlive::forward_server;
using namespace std;
using namespace media_manager;
using namespace http;

const int FSV2_TRACKER_KEEPALIVE_TIME = 20; // second
const int FSV2_TRACKER_NEXT_CONNECT_TIME = 2; // second
const int FSV2_TRACKER_NEXT_REGISTER_TIME = 2; // second
const int FSV2_STREAM_ALIVE_SEC = 5; // second #todo delete

static hashtable *g_streams = NULL;
static fs_stream* fs_get_stream(uint32_t id);

namespace interlive
{
namespace forward_server
{
  class ModTrackerFsv2Client : public ModTrackerClientBase
  {
  public:
    ModTrackerFsv2Client() : ModTrackerClientBase(),
      next_connect_time(0), next_register_time(0), next_keepalive_time(0), registered(false)
    {
      //ModTracker::get_inst().get_alive_connection();
      DBG("ModTrackerFsv2Client(), ptr=%lu", uint64_t(this));
    }

    virtual ~ModTrackerFsv2Client()
    {
      //ModTracker::get_inst().close_alive_connection();
      DBG("~ModTrackerFsv2Client(), ptr=%lu", uint64_t(this));
    }

    virtual int encode(const void* data, buffer* wb);

    virtual int decode(buffer* rb);

    virtual void on_error(short which)
    {
      WRN("ModTrackerFsv2Client::on_error, which=%d, nct=%d, nrt=%d, nkt=%d, reg=%d",
        int(which), int(next_connect_time), int(next_register_time), int(next_keepalive_time), int(registered));

      if (registered)
        reset_status();
    }

    void reset_status()
    {
      next_keepalive_time = 0;
      next_connect_time = time(0) + FSV2_TRACKER_NEXT_CONNECT_TIME;
      next_register_time = 0;
      registered = false;
    }

  public:
    time_t next_connect_time;
    time_t next_register_time;
    time_t next_keepalive_time;
    bool registered;
  };
} // namespace forward_server
} // namespace interlive

static ModTrackerFsv2Client* g_ModTrackerFsv2Client = NULL;


ForwardServer * ForwardServer::_g_server = NULL;
ForwardServer * ForwardServer::get_server()
{
  if (!_g_server)
  {
    _g_server = new ForwardServer;
  }
  return _g_server;
}


void ForwardServer::release()
{
  if (_g_server)
  {
    delete _g_server;
    _g_server = NULL;
  }
}

ForwardServer::ForwardServer() :
_dump_time_last(time(0)),
_max_session_num(1000)
{
}

ForwardServer::~ForwardServer()
{
  _destroy();
}

void ForwardServer::on_second(time_t t)
{
  if (g_ModTrackerFsv2Client == nullptr)
    return;

  if ((g_ModTrackerFsv2Client->next_connect_time != 0) &&
    (t >= g_ModTrackerFsv2Client->next_connect_time))
    _connect_tracker();

  if (!(g_ModTrackerFsv2Client->registered) &&
    (g_ModTrackerFsv2Client->next_register_time != 0) &&
    (t >= g_ModTrackerFsv2Client->next_register_time) &&
    (g_ModTrackerFsv2Client->next_connect_time == 0))
    _register_to_tracker();

  if (g_ModTrackerFsv2Client->registered &&
    (g_ModTrackerFsv2Client->next_keepalive_time != 0) &&
    (t >= g_ModTrackerFsv2Client->next_keepalive_time))
  {
    _tracker_keepalive();
  }
}

bool ForwardServer::create(const backend_config& backend_conf)
{
  try
  {
    using namespace media_manager;

    _backend_conf = &backend_conf;

    _dump_time_last = time(0);
    FlvCacheManager * cache_manager = FlvCacheManager::Instance();
    if (cache_manager == NULL)
    {
      WRN("ForwardServer::create:cache_manager is null!");
      return false;
    }
    cache_manager->register_watcher(_data_callback_by_cm,
      CACHE_WATCHING_CONFIG | CACHE_WATCHING_FLV_HEADER | CACHE_WATCHING_FLV_BLOCK |
      CACHE_WATCHING_FLV_FRAGMENT | CACHE_WATCHING_TS_SEGMENT | CACHE_WATCHING_SDP,
      this);

    if (-1 == init_forward_levels())
      return false;

    _init_tracker_conn();


    g_streams = hash_create(10);
    if (NULL == g_streams)
    {
      ERR("ForwardServer::create: g_streams is null!");
      return false;
    }
  }
  catch (...)
  {
    ERR("ForwardServer::create: Exception!");
    return false;
  }
  return true;
}

//void fs_get_streams_handler_wrapper(HTTPConnection* conn, void* data) {
//}
//
//void fs_get_streams_check_handler_wrapper(HTTPConnection* conn, void* data) {
//  if (conn->get_to_read() != 0)
//  {
//    return;
//  }
//  json_object* rsp = json_object_new_object();
//  conn->writeResponse(rsp);
//  json_object_put(rsp);
//  conn->stop();
//}

//void ForwardServer::set_http_server(HTTPServer* http_server) {
//  HTTPHandle* handle = http_server->add_handle("/fs_get_streams",
//    fs_get_streams_handler_wrapper,
//    this,
//    fs_get_streams_check_handler_wrapper,
//    this,
//    NULL,
//    NULL);
//  if (handle == NULL)
//  {
//    ERR("add_handle error");
//  }
//}

void ForwardServer::_init_tracker_conn()
{
  g_ModTrackerFsv2Client = new ModTrackerFsv2Client();
  g_ModTrackerFsv2Client->reset_status();
}

// call in   fs_data_callback_by_sm, fs_stream_timeout_handler
bool ForwardServer::_report_add_stream(StreamId_Ext stream_id)
{
  INF("ForwardServer::_report_add_stream: stream_id=%s", stream_id.unparse().c_str());

  // #TODO need implement tb_is_src_stream_v2 get_forward_level_v2
  if (tb_is_src_stream_v2(stream_id) == TRUE)
  {
    return _report_to_tracker(stream_id, 1, 0);
  }
  else
  {
    uint16_t level = -1;
    if (get_forward_level_v2(stream_id, &level) == 0 && level > 0)
    {
      return _report_to_tracker(stream_id, 1, level);
    }
    else
    {
      backend_config* cfg_backend = (backend_config*)ConfigManager::get_inst_config_module("backend");
      if (NULL == cfg_backend)
      {
        WRN("ForwardServer::_report_add_stream:cfg_backend is null!");
        return false;
      }

      WRN("ForwardServer::_report_add_stream: wrong level, stream_id: %s",
        stream_id.unparse().c_str());
      return _report_to_tracker(stream_id, 1, cfg_backend->backend_level);
    }
  }


  return false;
}

// 将stream信息上报给tracker
bool ForwardServer::_report_to_tracker(StreamId_Ext stream_id, int action, int level)
{
  if (!(g_ModTrackerFsv2Client->registered))
  {
    WRN("ForwardServer::_report_to_tracker: forward server not registered");
    return false;
  }

  proto_header ph = { 0, 0, CMD_FS2T_UPDATE_STREAM_REQ_V3, 0 };
  f2t_update_stream_req_v3 req;
  memmove(req.streamid, stream_id.id_char, STREAM_ID_LEN);
  req.cmd = action;
  req.level = level;

  proto_wrapper pw = { &ph, &req };
  bool ret = ModTracker::get_inst().send_request(&pw, g_ModTrackerFsv2Client);
  if (!ret)
  {
    DBG("ForwardServer::_report_to_tracker: reset_status");
    g_ModTrackerFsv2Client->reset_status();
  }

  INF("ForwardServer::_report_to_tracker: stream_id=%s, action=%d, level=%d, ret=%d", stream_id.unparse().c_str(), action, level, int(ret));
  return ret;
}

int ModTrackerFsv2Client::encode(const void* data, buffer* wb)
{
    if (NULL == data)
    {
        WRN("ModTrackerFsv2Client::encode:data is null!");
        return -1;
    }

    const proto_wrapper* packet = reinterpret_cast<const proto_wrapper*>(data);
    if (NULL == packet->header)
    {
        WRN("ModTrackerFsv2Client::encode: packet->header is null!");
        return -1;
    }

    proto_t cmd = packet->header->cmd;
    if (cmd <= 0)
    {
        WRN("ModTrackerFsv2Client::encode:cmd is invalid!");
        return -1;
    }

    int status = -1;
    int size = 0;

    switch (cmd)
    {
    case CMD_FS2T_REGISTER_REQ_V4:
    {
        F2TPBBuffer *pb_buff = (F2TPBBuffer*)(packet->payload);
        size = sizeof(proto_header)+pb_buff->size;
        status = encode_header_v3(wb, CMD_FS2T_REGISTER_REQ_V4, size);
        if (status != -1)
            status = buffer_append_ptr(wb, (void*)(pb_buff->buff), pb_buff->size);
        break;
    }
    case CMD_FS2T_UPDATE_STREAM_REQ_V3:
        size = sizeof(proto_header)+sizeof(f2t_update_stream_req_v3);
        status = encode_header_v3(wb, CMD_FS2T_UPDATE_STREAM_REQ_V3, size);
        if (status != -1)
            status = encode_f2t_update_stream_req_v3((f2t_update_stream_req_v3*)(packet->payload), wb);
        break;
    case CMD_FS2T_KEEP_ALIVE_REQ_V3:
        size = sizeof(proto_header)+sizeof(f2t_keep_alive_req_v3);
        status = encode_header_v3(wb, CMD_FS2T_KEEP_ALIVE_REQ_V3, size);
        if (status != -1)
            status = encode_f2t_keep_alive_req_v3((f2t_keep_alive_req_v3*)(packet->payload), wb);
        break;
    default:
        WRN("ModTrackerFsv2Client::encode:invalid cmd!");
        return -1;
    }

    DBG("ModTrackerFsv2Client::encode: cmd=%d, status=%d", int(cmd), int(status));

    if (status == -1)
    {
        return -1;
    }
    else
    {
        return size;
    }
}

int ModTrackerFsv2Client::decode(buffer* rb)
{
    if (NULL == rb)
    {
        WRN("ModTrackerFsv2Client::decode:rb is null!");
        return -1;
    }

    int ret = 0;

    proto_header header;
    if (decode_header(rb, &header) == -1)
    {
        WRN("ModTrackerFsv2Client::decode:decode_header error!");
        return -1;
    }
    ret += sizeof(proto_header);

    INF("ModTrackerFsv2Client::decode: cmd=%d", int(header.cmd));
    rb->_start += sizeof(proto_header);

    switch (header.cmd)
    {
    case CMD_FS2T_REGISTER_RSP_V3:
    {
        f2t_register_rsp_v3 msg;
        if (!decode_f2t_register_rsp_v3(&msg, rb))
        {
            WRN("ModTrackerFsv2Client::decode:decode_f2t_register_rsp_v3 error!");
            return -1;
        }

        ret += sizeof(f2t_register_rsp_v3);

        if (msg.result != TRACKER_RSP_RESULT_OK)
        {
            ERR("ModTrackerFsv2Client::decode: tracker result wrong, result=%d", int(msg.result));
        }
        else
        {
            // state machine move to register
            next_register_time = 0;
            registered = true;
            next_keepalive_time = time(nullptr);
            ForwardServer::get_server()->update_all_streams_to_tracker();//#todo test
        }
    }
        break;
    case CMD_FS2T_UPDATE_STREAM_RSP_V3:
    {
        f2t_update_stream_rsp_v3 msg;

        if (!decode_f2t_update_stream_rsp_v3(&msg, rb))
        {
            WRN("ModTrackerFsv2Client::decode:decode_f2t_update_stream_rsp_v3 error!");
            return -1;
        }

        ret += sizeof(f2t_update_stream_rsp_v3);

        if (msg.result != TRACKER_RSP_RESULT_OK)
        {
            ERR("ModTrackerFsv2Client::decode: tracker result wrong, result=%d", int(msg.result));
            //g_ModTrackerFsv2Client->reset_status();
        }
    }
        break;
    case CMD_FS2T_KEEP_ALIVE_RSP_V3:
    {
        f2t_keep_alive_rsp_v3 msg;
        if (!decode_f2t_keep_alive_rsp_v3(&msg, rb))
        {
            WRN("ModTrackerFsv2Client::decode:decode_f2t_keep_alive_rsp_v3 error!");
            return -1;
        }
        ret += sizeof(f2t_keep_alive_rsp_v3);

        if (msg.result != TRACKER_RSP_RESULT_OK)
        {
            ERR("ModTrackerFsv2Client::decode: tracker result wrong, result=%d", int(msg.result));
            //g_ModTrackerFsv2Client->reset_status();
        }
    }
        break;
    default:
        WRN("ModTrackerFsv2Client::decode:invalid cmd!");
        return -1;
    }

    rb->_start -= sizeof(proto_header);
    return ret;
}

// used in module_backend
void ForwardServer::add_stream_to_tracker(StreamId_Ext stream_id, int level)
{
  DBG("ForwardServer::add_stream_to_tracker: stream_id=%s, level=%d", stream_id.unparse().c_str(), level);
  _report_to_tracker(stream_id, 1, level);
}

// used in module_backend
void ForwardServer::del_stream_from_tracker(StreamId_Ext stream_id, int level)
{
  DBG("ForwardServer::del_stream_from_tracker: stream_id=%s, level=%d", stream_id.unparse().c_str(), level);
  //#todo correspond insert/erase in _data_callback_by_cm, _stream_timeout_callback_by_cm
  _fs_streams.erase(stream_id);

  _report_to_tracker(stream_id, 0, level);
}

void ForwardServer::update_all_streams_to_tracker()
{
  DBG("ForwardServer::update_all_streams_to_tracker: count=%lu", _fs_streams.size());

  if (!(g_ModTrackerFsv2Client->registered))
  {
    WRN("ForwardServer::update_all_streams_to_tracker: forward server not registered");
    return;
  }

  for (fs_streams_t::iterator iter = _fs_streams.begin(); iter != _fs_streams.end(); ++iter)
  {
    _report_add_stream(iter->first);
  }
}

uint32_t ForwardServer::get_stream_count() const
{
  return _fs_streams.size();
}

bool ForwardServer::has_register_tracker() const
{
  return g_ModTrackerFsv2Client->registered;
}

void ForwardServer::_connect_tracker()
{
  TRC("ForwardServer::_connect_tracker: _connect_tracker");

  ModTracker& tracker_inst(ModTracker::get_inst());
  tracker_inst.get_alive_connection();
  if (tracker_inst.connection_alive())
  {
    DBG("ForwardServer::_connect_tracker: _connect_tracker: connection_alive");
    // state machine move to register
    g_ModTrackerFsv2Client->next_connect_time = 0;
    g_ModTrackerFsv2Client->next_register_time = time(nullptr) + FSV2_TRACKER_NEXT_REGISTER_TIME;
  }
  else
  {
    // check if connected next time
    g_ModTrackerFsv2Client->next_connect_time = time(nullptr) + FSV2_TRACKER_NEXT_CONNECT_TIME;
  }
}

// 将本机的ip/port注册给tracker
void ForwardServer::_register_to_tracker()
{
  proto_header ph = { 0, 0, CMD_FS2T_REGISTER_REQ_V4, 0 };
  f2t_register_req_v4 req;
  F2TPBBuffer pb_buff;

  req.set_transport_type(LCdnStreamTransportRtp);
  req.set_backend_port(_backend_conf->backend_listen_port);
  req.set_ip(inet_addr(_backend_conf->backend_listen_ip));
  INF("ForwardServer::_register_to_tracker:ip:%s", _backend_conf->backend_listen_ip);

  req.set_asn(_backend_conf->backend_asn);
  req.set_region(_backend_conf->backend_region);

  HTTPServerConfig *_http_config = (HTTPServerConfig *)ConfigManager::get_inst_config_module("http_server");
  if (NULL != _http_config)
  {
    uint32_t player_port = _http_config->listen_port;
    req.set_player_port(player_port);
  }

  uploader_config *uploader_c = (uploader_config*)ConfigManager::get_inst_config_module("uploader");
  if (NULL != uploader_c)
  {
    uint32_t uploader_port = uploader_c->listen_port;
    req.set_uploader_port(uploader_port);
  }

  uint32_t public_ip[2];
  int public_ip_cnt = 0;
  uint32_t private_ip[2];
  int private_ip_cnt = 0;
  int rtn = -1;

  rtn = util_get_ip(public_ip, 2, &public_ip_cnt, util_ipfilter_public);
  if (0 != rtn && public_ip_cnt < 1)
  {
    if (rtn > 0)
    {
      ERR("ForwardServer::_register_to_tracker: warning...get public ip failed. error = %s", strerror(errno));
    }
    else
    {
      ERR("ForwardServer::_register_to_tracker: warning...get public ip failed. ret = %d, cnt = %d", rtn, public_ip_cnt);
    }
  }
  else
  {
    for (int i = 0; i < public_ip_cnt; i++)
    {
      req.add_public_ip(public_ip[i]);
    }
  }

  rtn = util_get_ip(private_ip, 2, &private_ip_cnt, util_ipfilter_private);
  if (0 != rtn && private_ip_cnt < 1)
  {
    if (rtn > 0)
    {
      ERR("ForwardServer::_register_to_tracker: warning...get private ip failed. error = %s", strerror(errno));
    }
    else
    {
      ERR("ForwardServer::_register_to_tracker: warning...get private ip failed. ret = %d, cnt = %d", rtn, private_ip_cnt);
    }
  }
  else
  {
    for (int i = 0; i < private_ip_cnt; i++)
    {
      req.add_public_ip(private_ip[i]);
    }
  }

  pb_buff.size = req.ByteSize();
  req.SerializeToArray(pb_buff.buff, pb_buff.size);

  proto_wrapper pw = { &ph, (void*)&pb_buff };
  bool ret = ModTracker::get_inst().send_request(&pw, g_ModTrackerFsv2Client);
  if (ret)
  {
    DBG("ForwardServer::_register_to_tracker: reset_status");
    g_ModTrackerFsv2Client->reset_status();
  }
  else
  {
    g_ModTrackerFsv2Client->next_register_time = time(nullptr) + FSV2_TRACKER_NEXT_REGISTER_TIME;
  }

  INF("ForwardServer::_register_to_tracker: ret=%d", int(ret));
}

void ForwardServer::_tracker_keepalive()
{
  proto_header ph = { 0, 0, CMD_FS2T_KEEP_ALIVE_REQ_V3, 0 };
  f2t_keep_alive_req_v3 msg;
  msg.fid = 0;
  proto_wrapper pw = { &ph, &msg };
  bool ret = ModTracker::get_inst().send_request(&pw, g_ModTrackerFsv2Client);

  if (ret)
  {
    g_ModTrackerFsv2Client->next_keepalive_time = time(nullptr) + FSV2_TRACKER_KEEPALIVE_TIME;
  }
  else
  {
    g_ModTrackerFsv2Client->reset_status();
    DBG("ForwardServer::_tracker_keepalive: reset_status");
  }
}

void ForwardServer::_data_callback_by_cm(StreamId_Ext stream_id, uint8_t watch_type, void* arg)
{
  if (NULL == arg)
  {
    WRN("ForwardServer::_data_callback_by_cm:arg is null!");
    return;
  }

  if (stream_id.get_32bit_stream_id() == 0)
  {
    WRN("ForwardServer::_data_callback_by_cm:stream_id.get_32bit_stream_id is 0!");
    return;
  }

  //INF("ForwardServer::_data_callback_by_cm, stream_id=%s", stream_id.unparse().c_str());

  ForwardServer* _this = static_cast<ForwardServer*>(arg);
  fs_streams_t& _fs_streams(_this->_fs_streams);

  fs_streams_t::iterator iter_fsstream = _fs_streams.find(stream_id);
  if (iter_fsstream == _fs_streams.end())
  {
    bool ret = _this->_report_add_stream(stream_id);

    FSStream fsstream;
    fsstream.streamid = stream_id;
    fsstream.last_active_time = time(nullptr);
    fsstream.alive = ret;

    _fs_streams[stream_id] = fsstream;
  }
  else
  {
    iter_fsstream->second.last_active_time = time(nullptr);

    if (!iter_fsstream->second.alive)
    {
      bool ret = _this->_report_add_stream(stream_id);
      if (ret)
        iter_fsstream->second.alive = true;
    }
  }
}

void ForwardServer::_destroy()
{
  if (NULL != g_streams)
  {
    hash_destroy(g_streams, NULL);
    g_streams = NULL;
  }
}

void ForwardServer::_write()
{
}

static fs_stream* fs_get_stream(uint32_t id)
{
  hashnode *node = hash_get(g_streams, id);
  if (node)
  {
    return container_of(node, fs_stream, hash_entry);
  }
  return NULL;
}

int fs_start_stream(uint32_t stream_id)
{
  INF("fs_start_stream. stream_id: %u", stream_id);
  fs_stream *stream = fs_get_stream(stream_id);
  if (NULL != stream)
  {
    WRN("fs_start_stream: stream_id: %u has already existed", stream_id);
    return 0;
  }

  stream = (fs_stream*)mmalloc(sizeof(fs_stream));
  if (NULL == stream)
  {
    ERR("fs_start_stream: %s", "mmalloc stream error.");
    return -1;
  }
  stream->stream_id = stream_id;
  stream->sessions = NULL;
  stream->session_count = 0;
  stream->alive = 0;
  SESSION_INIT(&stream->timeout_ses, NULL, 0);
  hash_insert(g_streams, stream_id, &stream->hash_entry);
  return 0;
}

int fs_stop_stream(uint32_t stream_id)
{
  INF("fs_stop_stream: stream_id=%d", stream_id);
  hashnode* node = hash_delete(g_streams, stream_id);
  if (node == NULL)
  {
    WRN("fs_stop_stream: no such stream stream_id=%u", stream_id);
    return 0;
  }
  fs_stream* stream = container_of(node, fs_stream, hash_entry);
  // hash_for_each(stream->sessions, fs_session, stream_entry, fs_close_session);
  // hash_destroy(stream->sessions, NULL);
  session_manager_detach(&stream->timeout_ses);
  mfree(stream);
  stream = NULL;
  return 0;
}
