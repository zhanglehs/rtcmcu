/**
* @file  module_tracker.cpp
* @brief communication with tracker (v2)
*
* @author houxiao
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>houxiao@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/12/22
* @see  module_tracker.h
*/

#include "module_tracker.h"
#include "util/levent.h"
#include "util/log.h"
#include "util/xml.h"

#include <assert.h>
#include <vector>
#include <arpa/inet.h>

static ModTracker* g_ModTracker = nullptr;
ModTrackerClientBase* ModTracker::_default_inbound_client = NULL;

ModTrackerConfig::ModTrackerConfig()
{
  inited = false;
  memset(tracker_ip, 0, sizeof(tracker_ip));
  tracker_port = 0;
  memset(tracker_backup_ip, 0, sizeof(tracker_backup_ip));
  tracker_backup_port = 0;
}

ModTrackerConfig& ModTrackerConfig::operator=(const ModTrackerConfig& rhv)
{
  memmove(this, &rhv, sizeof(ModTrackerConfig));
  return *this;
}

void ModTrackerConfig::set_default_config()
{
  strcpy(tracker_ip, "127.0.0.1");
  tracker_port = 1841;
  strcpy(tracker_backup_ip, "127.0.0.1");
  tracker_backup_port = 1841;
  tracker_http_port = 80;
}

bool ModTrackerConfig::load_config(xmlnode* xml_config)
{
  ASSERTR(xml_config != nullptr, false);
  xmlnode *p = xmlgetchild(xml_config, "tracker", 0);
  ASSERTR(p != nullptr, false);

  if (inited)
    return true;

  char *q = nullptr;

  q = xmlgetattr(p, "tracker_ip");
  if (!q)
  {
    ERR("tracker listen_host get failed.");
    return false;
  }
  strncpy(tracker_ip, q, 31);

  q = xmlgetattr(p, "tracker_port");
  if (!q)
  {
    ERR("tracker listen_port get failed.");
    return false;
  }
  tracker_port = atoi(q);
  if (tracker_port <= 0)
  {
    ERR("tracker tacker_port not valid.");
    return false;
  }

  q = xmlgetattr(p, "tracker_http_port");
  if (!q)
  {
    ERR("tracker tracker_http_port not set");
    return false;
  }
  tracker_http_port = atoi(q);
  if (tracker_http_port <= 0)
  {
    ERR("tracker tacker_http_port not valid.");
    return false;
  }


  q = xmlgetattr(p, "tracker_backup_ip");
  if (!q)
  {
    ERR("tracker backup listen_host get failed.");
    return false;
  }
  strncpy(tracker_backup_ip, q, 31);

  q = xmlgetattr(p, "tracker_backup_port");
  if (!q)
  {
    ERR("tracker backup listen_port get failed.");
    return false;
  }
  tracker_backup_port = atoi(q);
  if (tracker_backup_port <= 0)
  {
    ERR("tracker backup tacker_port not valid.");
    return false;
  }

  inited = true;
  return true;
}

bool ModTrackerConfig::reload() const
{
  return true;
}

const char* ModTrackerConfig::module_name() const
{
  static const char* name = "tracker";
  return name;
}

void ModTrackerConfig::dump_config() const
{
  INF("tracker config: " "tracker_ip=%s, tracker_port=%hu", tracker_ip, tracker_port);
}

ModTrackerConnection::ModTrackerConnection() :
errorno(0), last_keepalive_time(0), registered(false), conn()
{
  conn.state = CONN_STATE_NOT_INIT;
}

void ModTrackerConnection::reset_keepalive()
{
  last_keepalive_time = 0;
  registered = false;
}

void ModTrackerConnection::close_connection()
{
  ::close_connection(&conn);
  conn.state = CONN_STATE_NOT_INIT;
}

void ModTrackerConnection::reset_connection()
{
  ::reset_connection(&conn);
}

ModTrackerSessionParam::ModTrackerSessionParam(module_t _mod, proto_t _rsp_cmd, stream_id_t _streamid) :
mod(_mod), rsp_cmd(_rsp_cmd), streamid(_streamid), create_time(time(nullptr))
{
}

ModTrackerOnReadSession::ModTrackerOnReadSession() : param(), header(nullptr), tracker_header(nullptr), event(0)
{
}

ModTrackerOnConnectSession::ModTrackerOnConnectSession() :
cbfunc(nullptr), param()
{
}

ModTrackerOnConnectSession::ModTrackerOnConnectSession(tracker_on_connect_cbfunc_t _cbfunc, ModTrackerSessionParam _param) :
cbfunc(_cbfunc), param(_param)
{
}

ModTrackerClientParam::ModTrackerClientParam(ModTrackerClientBase* _client /*= nullptr*/) :
client(_client), create_time(time(nullptr)), debug_seq(0)
{
}

ModTracker& ModTracker::get_inst()
{
  if (g_ModTracker == nullptr)
    g_ModTracker = new ModTracker();
  return *g_ModTracker;
}

void ModTracker::destory_inst()
{
  if (g_ModTracker != nullptr)
  {
    delete g_ModTracker;
    g_ModTracker = nullptr;
  }
}

ModTracker::ModTracker() :
_is_backup_ip_used(false), _config(nullptr), _main_base(nullptr), _tracker_conn(), _seq(0),
_last_check_session_timeout(time(nullptr)), _cb_on_connect_session(), _on_read_cbfunc_reg(),
_cb_on_read_session(), _on_error_cbfunc_reg(), _on_read_cbobj_reg()
{
}

int ModTracker::tracker_init(struct event_base * main_base, const ModTrackerConfig * config)
{
  if (main_base == nullptr || config == nullptr)
    return -1;

  INF("ModTracker::tracker_init: init tracker conn, ip=%s, port=%u", config->tracker_ip, config->tracker_port);
  _config = config;
  _main_base = main_base;

  _tracker_conn.reset_keepalive();
  return _init_connection();
}

void ModTracker::tracker_on_second(time_t t)
{
  _check_timeout(t);
}

void ModTracker::tracker_fini()
{
  _tracker_conn.close_connection();
  clearup_module_register();
}

tracker_on_read_cbfunc_t ModTracker::register_on_read_callback(tracker_on_read_cbfunc_t callback_func, TrackerModProto mp)
{
  on_read_cbfunc_map_t::iterator iter_old_func = _on_read_cbfunc_reg.find(mp);
  if (iter_old_func == _on_read_cbfunc_reg.end())
  {
    if (callback_func != nullptr)
      _on_read_cbfunc_reg[mp] = callback_func;
    return callback_func;
  }
  else
  {
    tracker_on_read_cbfunc_t old_func = iter_old_func->second;

    if (callback_func == nullptr)
      _on_read_cbfunc_reg.erase(iter_old_func);
    else
      iter_old_func->second = callback_func;
    return old_func;
  }
}

tracker_on_connect_cbfunc_t ModTracker::register_on_connect_callback(tracker_on_connect_cbfunc_t callback_func, ModTrackerSessionParam param)
{
  if (callback_func == nullptr)
    return nullptr;

  if (_tracker_conn.conn.state == CONN_STATE_CONNECTED)
  {
    // call back directly
    callback_func(_tracker_conn, param);
  }
  else
  {
    // wait for connected
    _cb_on_connect_session.push_back(ModTrackerOnConnectSession(callback_func, param));
  }

  return callback_func;
}

tracker_on_error_cbfunc_t ModTracker::register_on_error_callback(tracker_on_error_cbfunc_t callback_func, module_t mod)
{
  on_error_cbfunc_map_t::iterator iter_old_func = _on_error_cbfunc_reg.find(mod);

  if (iter_old_func == _on_error_cbfunc_reg.end())
  {
    if (callback_func != nullptr)
      _on_error_cbfunc_reg[mod] = callback_func;
    return callback_func;
  }
  else
  {
    tracker_on_error_cbfunc_t old_func = iter_old_func->second;

    if (callback_func == nullptr)
      _on_error_cbfunc_reg.erase(iter_old_func);
    else
      iter_old_func->second = callback_func;
    return old_func;
  }
}

void ModTracker::clearup_module_register(module_t mod)
{
  if (mod == MT__FIRST)
  {
    _cb_on_connect_session.clear();
    _on_read_cbfunc_reg.clear();
    _cb_on_read_session.clear();
    _on_error_cbfunc_reg.clear();
    _on_read_cbobj_reg.clear();
  }
  else
  {
    // TODO
  }
}

ModTrackerConnection& ModTracker::get_alive_connection()
{
  _make_connection_alive();
  return _tracker_conn;
}

bool ModTracker::close_alive_connection()
{
  // if connection error then we should close connection actually
  // and make all sessions timeout

  if (_tracker_conn.errorno == 0)
    return false; // do nothing

  // connection state: any => CONN_STATE_NOT_INIT
  _tracker_conn.close_connection();

  for (on_read_cbobj_map_t::iterator iterClient = _on_read_cbobj_reg.begin();
    iterClient != _on_read_cbobj_reg.end(); ++iterClient)
  {
    ModTrackerClientParam& param(iterClient->second);
    param.create_time = 0;
  }

  for (on_conn_sess_vec_t::iterator iter_conn_sess = _cb_on_connect_session.begin();
    iter_conn_sess != _cb_on_connect_session.end(); ++iter_conn_sess)
  {
    ModTrackerSessionParam& param(iter_conn_sess->param);
    param.create_time = 0;
  }

  for (on_read_sess_map_t::iterator iter_read_sess = _cb_on_read_session.begin();
    iter_read_sess != _cb_on_read_session.end(); ++iter_read_sess)
  {
    ModTrackerSessionParam& param(iter_read_sess->second);
    param.create_time = 0;
  }

  return true;
}

bool ModTracker::connection_alive() const
{
  return (_tracker_conn.conn.state == CONN_STATE_CONNECTED) && (_tracker_conn.errorno == 0);
}

bool ModTracker::_make_connection_alive()
{
  connection_base& conn(_tracker_conn.conn);

  if (conn.state == CONN_STATE_CONNECTING)
    return false;

  if (conn.state == CONN_STATE_NOT_INIT)
  {
    if (!_init_connection())
      return false;
    /* else do nothing */
  }

  if (conn.state == CONN_STATE_INIT)
  {
    if (do_connect(&conn) == -1)
    {
      _reset_connection();
      ERR("ModTracker::_make_connection_alive: connect tracker error, ip=%s, port=%u", _config->tracker_ip, _config->tracker_port);
      return false;
    }
    else
    {
      return false;
    }
  }

  if (conn.state == CONN_STATE_CONNECTED)
    return true;

  return false;
}

void ModTracker::_deal_with_seq_overflow()
{
  if (_seq < TRACKER_HEADER_SEQ_OVERFLOW_MAX)
    return;

  // deal with timeout sessions
  _check_timeout(time(nullptr));
  _seq = 0;
}

bool ModTracker::_init_connection()
{
  ASSERTR(_config != nullptr, false);

  _tracker_conn.errorno = 0;
  connection_base* conn = &(_tracker_conn.conn);
  bool ret = (init_connection_by_addr(conn, ntohl(inet_addr(_config->tracker_ip)), _config->tracker_port) == 0);
  // connection state: CONN_STATE_NOT_INIT => CONN_STATE_INIT
  conn->on_connect = _on_connect;
  conn->on_read = _on_read;
  conn->on_error = _on_error;

  return ret;
}

bool ModTracker::_reset_connection()
{
  ASSERTR(_config != nullptr, false);

  _tracker_conn.errorno = 0;
  connection_base* conn = &(_tracker_conn.conn);
  bool ret = false;
  if (!_is_backup_ip_used)
  {
    ret = (init_connection_by_addr(conn, ntohl(inet_addr(_config->tracker_ip)), _config->tracker_port) == 0);
    _is_backup_ip_used = true;
  }
  else
  {
    ret = (init_connection_by_addr(conn, ntohl(inet_addr(_config->tracker_backup_ip)), _config->tracker_backup_port) == 0);
    _is_backup_ip_used = false;
  }
  // connection state: CONN_STATE_NOT_INIT => CONN_STATE_INIT
  conn->on_connect = _on_connect;
  conn->on_read = _on_read;
  conn->on_error = _on_error;

  return ret;
}

// 所有与tracker交互的消息都有超时检测。这里的超时检测方法很low
size_t ModTracker::_check_timeout(time_t curr_time)
{
  const time_t check_time = curr_time - TRACKER_SESSION_TIMEOUT;
  if (_last_check_session_timeout > check_time)
  {
    return 0;
  }
  else
  {
    _last_check_session_timeout = curr_time;

    size_t total_cnt = 0;
    total_cnt += _check_timeout_client(check_time);
    total_cnt += _check_timeout_session(check_time);
    return total_cnt;
  }
}

size_t ModTracker::_check_timeout_client(time_t check_time)
{
  if (_on_read_cbobj_reg.empty())
  {
    return 0;
  }
  else
  {
    DBG("ModTracker::_check_timeout_client, _on_read_cbobj_reg_size=%d", int(_on_read_cbobj_reg.size()));
  }

  size_t cnt = 0;

  tracker_client_vec_t tracker_clients;
  tracker_clients.reserve(_on_read_cbobj_reg.size());

  for (on_read_cbobj_map_t::iterator iter_client = _on_read_cbobj_reg.begin();
    iter_client != _on_read_cbobj_reg.end();)
  {
    const ModTrackerClientParam& param(iter_client->second);
    if (param.create_time > check_time)
    {
      ++iter_client;
    }
    else
    {
      cnt++;
      tracker_clients.push_back(param);
      _on_read_cbobj_reg.erase(iter_client++);
    }
  }

  for (tracker_client_vec_t::iterator iter_client = tracker_clients.begin();
    iter_client != tracker_clients.end(); ++iter_client)
  {
    const ModTrackerClientParam& param(*iter_client);
    if (param.client != nullptr)
    {
      WRN("ModTracker::_check_timeout_client, seq=%d, p_ct=%d", int(param.debug_seq), int(param.create_time));
      param.client->on_error(_tracker_conn.errorno);
    }
  }

  DBG("ModTracker::_check_timeout_client, cnt=%d", int(cnt));
  return cnt;
}

size_t ModTracker::_check_timeout_session(time_t check_time)
{
  // check the create_time of every session
  // group in vector by module id
  // call the cbfunc

  if (_cb_on_connect_session.empty() && _cb_on_read_session.empty())
    return 0;

  size_t cnt = 0;
  tracker_session_vec_t mod_error_sessions[MT__LAST];

  for (on_conn_sess_vec_t::iterator iter_conn_sess = _cb_on_connect_session.begin();
    iter_conn_sess != _cb_on_connect_session.end();)
  {
    ModTrackerSessionParam& param(iter_conn_sess->param);
    if (param.create_time >= check_time)
    {
      ++iter_conn_sess;
      continue;
    }

    cnt++;

    module_t mod = param.mod;
    ASSERTR(mod > MT__FIRST && mod < MT__LAST, 0);
    mod_error_sessions[mod].push_back(param);

    iter_conn_sess = _cb_on_connect_session.erase(iter_conn_sess);
  }

  for (on_read_sess_map_t::iterator iter_read_sess = _cb_on_read_session.begin();
    iter_read_sess != _cb_on_read_session.end();)
  {
    ModTrackerSessionParam& param(iter_read_sess->second);
    if (param.create_time > check_time)
    {
      ++iter_read_sess;
    }
    else
    {
      cnt++;

      //f2t_v2_seq_t seq = iter_read_sess->first; // debug
      module_t mod = param.mod;
      ASSERTR(mod > MT__FIRST && mod < MT__LAST, 0);
      mod_error_sessions[mod].push_back(param);

      _cb_on_read_session.erase(iter_read_sess++);
    }
  }

  if (cnt == 0)
    return 0;

  for (int mod = MT__FIRST + 1; mod < MT__LAST; mod++)
  {
    tracker_session_vec_t& sessions(mod_error_sessions[mod]);
    if (sessions.empty())
      continue;

    on_error_cbfunc_map_t::iterator iter_err_cbfunc = _on_error_cbfunc_reg.find(module_t(mod));
    if (iter_err_cbfunc != _on_error_cbfunc_reg.end())
    {
      tracker_on_error_cbfunc_t cbfunc = iter_err_cbfunc->second;
      cbfunc(_tracker_conn, sessions);
    }
    else
    {
      WRN("ModTracker::_check_timeout_session: tracker on error callback function not register, mod=%hu", int(mod));
    }
  }

  return cnt;
}

void ModTracker::_on_connect(connection_base* c)
{
  DBG("tracker connected");
  if (g_ModTracker->_cb_on_connect_session.empty())
    return;

  // this allows cbfunc cascading invoke register_on_connect_callback
  on_conn_sess_vec_t copy_of_conn_session(g_ModTracker->_cb_on_connect_session);
  g_ModTracker->_cb_on_connect_session.clear();

  for (on_conn_sess_vec_t::iterator iter = copy_of_conn_session.begin();
    iter != copy_of_conn_session.end(); ++iter)
  {
    tracker_on_connect_cbfunc_t cbfunc = iter->cbfunc;
    ASSERTV(cbfunc != nullptr);

    cbfunc(g_ModTracker->_tracker_conn, iter->param);
  }
}

void ModTracker::_on_read(connection_base* c, proto_t cmd)
{
  const int ph_and_mt2tph_size = sizeof(proto_header)+sizeof(mt2t_proto_header);

  // read packet
  proto_header header;
  if (decode_header(c->rb, &header) != 0)
  {
    WRN("ModTracker::_on_read: tracker on read decode_header error");
    buffer_reset(c->rb);
    return;
  }

  mt2t_proto_header tracker_header;
  int dec_ret = -1;
  if (header.version == VER_2)
    dec_ret = decode_f2t_header_v2(c->rb, &header, &tracker_header);
  else
    dec_ret = decode_f2t_header_v3(c->rb, &header, &tracker_header);

  if (dec_ret != 0)
  {
    WRN("ModTracker::_on_read: tracker on read decode_f2t_header_v3 error");
    buffer_reset(c->rb);
    return;
  }

  // retrieve client
  on_read_cbobj_map_t::iterator iter_client = g_ModTracker->_on_read_cbobj_reg.find(tracker_header.seq);
  on_read_sess_map_t::iterator iter_session = g_ModTracker->_cb_on_read_session.end();
  if (tracker_header.seq == 0)
  {
    if (NULL != _default_inbound_client)
    {
      c->rb->_start += ph_and_mt2tph_size;
      _default_inbound_client->decode(c->rb);
      c->rb->_start -= ph_and_mt2tph_size;
    }
  }
  else if (iter_client != g_ModTracker->_on_read_cbobj_reg.end())
  {
    const ModTrackerClientParam& param(iter_client->second);
    g_ModTracker->_on_read_cbobj_reg.erase(iter_client);

    ModTrackerClientBase* client = param.client;
    if (client != nullptr)
    {
      // callback
      c->rb->_start += ph_and_mt2tph_size;
      client->decode(c->rb);
      c->rb->_start -= ph_and_mt2tph_size;
    }
  }

  // retrieve session
  else if ((iter_session = g_ModTracker->_cb_on_read_session.find(tracker_header.seq)) !=
    g_ModTracker->_cb_on_read_session.end())
  {
    ModTrackerOnReadSession session;
    session.param = iter_session->second;
    g_ModTracker->_cb_on_read_session.erase(iter_session);

    session.event = EV_READ;
    session.header = &header;
    session.tracker_header = &tracker_header;
    INF("ModTracker::_on_read: cmd=%d, seq=%d, sp_ct=%u",
      int(header.cmd), int(tracker_header.seq), uint32_t(session.param.create_time));

    // find cbfunc
    on_read_cbfunc_map_t::iterator iter_cbfunc =
      g_ModTracker->_on_read_cbfunc_reg.find(TrackerModProto(session.param.mod, cmd));
    if (iter_cbfunc != g_ModTracker->_on_read_cbfunc_reg.end())
    {
      // callback
      tracker_on_read_cbfunc_t cbfunc = iter_cbfunc->second;
      ASSERTV(cbfunc != nullptr);
      cbfunc(g_ModTracker->_tracker_conn, session);
    }
  }

  // client/session not found
  else
  {
    WRN("ModTracker::_on_read: tracker on read callback function not register, cmd=%d", int(cmd));
  }
}

void ModTracker::_on_error(connection_base* c, short which)
{
  const char* type = which & EV_READ ? "read" : "write";
  ERR("ModTracker::_on_error: tracker conn error type=%s", type);

  g_ModTracker->_tracker_conn.errorno = which;
  g_ModTracker->close_alive_connection();
}

ModTrackerConnection& ModTracker::get_connection()
{
  return _tracker_conn;
}

f2t_v2_seq_t ModTracker::get_next_seq()
{
  _deal_with_seq_overflow();

  return ++_seq;
}

bool ModTracker::send_request(const void* data, ModTrackerClientBase* client, int ver /*= VER_3*/)
{
  ASSERTR(data != nullptr, false);
  ASSERTR(client != nullptr, false);
  ASSERTR(ver == VER_2 || ver == VER_3, false);

  if (!_make_connection_alive())
  {
    WRN("ModTracker::send_request: _make_connection_alive fail, conn_state=%d", int(_tracker_conn.conn.state));
    return false;
  }
  _deal_with_seq_overflow();
  ++_seq;

  connection_base& conn(_tracker_conn.conn);
  buffer* obuf = conn.wb;

  // write to output buffer
  uint32_t* psize = nullptr;
  int encode_header_ret = -1;
  if (ver == VER_2)
    encode_header_ret = encode_header_v2_s(obuf, CMD_MT2T_REQ_RSP, psize);
  else if (ver == VER_3)
    encode_header_ret = encode_header_v3_s(obuf, CMD_MT2T_REQ_RSP, psize);
  //ASSERTR(encode_header_ret >= 0 && psize != nullptr, (buffer_reset(obuf), false));
  ASSERTSR(encode_header_ret >= 0 && psize != nullptr, { buffer_reset(obuf); }, false);

  mt2t_proto_header tracker_header = { _seq };
  int encode_f2t_header_ret = encode_mt2t_header(obuf, tracker_header);
  ASSERTSR(encode_f2t_header_ret >= 0, { buffer_reset(obuf); }, false);

  int encode_payload_ret = client->encode(data, obuf);
  ASSERTSR(encode_payload_ret >= 0, { buffer_reset(obuf); }, false);

  *psize = htonl(sizeof(proto_header)+sizeof(tracker_header)+encode_payload_ret);
  ASSERTSR(buffer_data_len(obuf) >= 0, { buffer_reset(obuf); }, false);

  // register session (if write failed, session will timeout in a correct way)
  ModTrackerClientParam param(client);
  param.debug_seq = _seq;
  _on_read_cbobj_reg[_seq] = param;

  enable_write(&(_tracker_conn.conn));
  //buffer_try_adjust(_tracker_conn.conn.wb);
  return true;
}

void ModTracker::set_default_inbound_client(ModTrackerClientBase *client)
{
  _default_inbound_client = client;
}

ModTrackerClientBase::ModTrackerClientBase()
{
  // if need on connect callback, register to ModTracker.
}

ModTrackerClientBase::~ModTrackerClientBase()
{
  // unregister to ModTracker.
}
