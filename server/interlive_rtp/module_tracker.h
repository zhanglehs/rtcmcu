/**
* @file  module_tracker.h
* @brief communication with tracker (v2)
*
* @author houxiao
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>houxiao@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/12/22
*/

#ifndef MODULE_TRACKER1_H_
#define MODULE_TRACKER1_H_

#ifndef nullptr
#define nullptr NULL
#endif

#define TRACKER_HEADER_SEQ_OVERFLOW_MAX (USHRT_MAX - 10)
#define TRACKER_SESSION_TIMEOUT 5 // second

#include "common/type_defs.h"
#include "common/proto_define.h"
#include "common/proto.h"
#include "backend_new/forward_common.h"
#include "config_manager.h"

#include <stdint.h>
#include <event.h>
#include <map>
#include <vector>

typedef enum
{
    MT__FIRST,

    MT_BACKEND,
    MT_FORWARD,
    MT_FORWARD_CLIENT,
    MT_FORWARD_SERVER,
    MT_FORWARD_CLIENT_V2,
    MT_FORWARD_SERVER_V2,
    MT_PLAYER,
    MT_UPLOADER,
    MT_RECEIVER,
    MT_RESERVED,

    MT__LAST
} module_t;

// pre-definitions
struct ModTrackerConnection;
struct ModTrackerSessionParam;
struct ModTrackerOnReadSession;
struct ModTrackerOnErrorSession;
class ModTrackerClientBase;
struct ModTrackerClientParam;

typedef std::vector<ModTrackerSessionParam> tracker_session_vec_t;
typedef std::vector<ModTrackerClientParam> tracker_client_vec_t;

typedef void(*tracker_on_read_cbfunc_t)(ModTrackerConnection& tracker_conn, ModTrackerOnReadSession& session);
typedef void(*tracker_on_connect_cbfunc_t)(ModTrackerConnection& tracker_conn, ModTrackerSessionParam& param);
typedef void(*tracker_on_error_cbfunc_t)(ModTrackerConnection& tracker_conn, tracker_session_vec_t& sessions);

class ModTrackerConfig : public ConfigModule
{
private:
    bool inited;

public:
    char tracker_ip[32];
    uint16_t tracker_port;
    uint16_t tracker_http_port;
    char tracker_backup_ip[32];
    uint16_t tracker_backup_port;

    ModTrackerConfig();
    virtual ~ModTrackerConfig() {}
    ModTrackerConfig& operator=(const ModTrackerConfig& rhv);

    virtual void set_default_config();
    virtual bool load_config(xmlnode* xml_config);
    virtual bool reload() const;
    virtual const char* module_name() const;
    virtual void dump_config() const;
};

struct ModTrackerConnection
{
    short errorno;
    time_t last_keepalive_time;
    bool registered;
    connection_base conn;

    ModTrackerConnection();
    void reset_keepalive();
    void close_connection();
    void reset_connection();
};

struct TrackerModProto
{
    module_t mod;
    proto_t cmd;

    TrackerModProto() : mod(MT__FIRST), cmd(0) { }
    TrackerModProto(module_t _mod, proto_t _cmd) : mod(_mod), cmd(_cmd) { }

    bool operator < (const TrackerModProto& rhv) const
    {
        if (this->mod < rhv.mod)
            return true;
        else if (this->mod == rhv.mod)
            return (this->cmd < rhv.cmd);
        else
            return false;
    }
};

struct ModTrackerSessionParam
{
    module_t mod;
    proto_t rsp_cmd;
    stream_id_t streamid;
    time_t create_time;

    explicit ModTrackerSessionParam(module_t _mod = MT__FIRST, proto_t _rsp_cmd = -1, stream_id_t _streamid = 0);
};

struct ModTrackerClientParam
{
    ModTrackerClientBase* client;
    time_t create_time;
    f2t_v2_seq_t debug_seq;

    explicit ModTrackerClientParam(ModTrackerClientBase* _client = nullptr);
};

struct ModTrackerOnReadSession
{
    ModTrackerSessionParam param;
    const proto_header* header; // reset by callback
    const mt2t_proto_header* tracker_header; // reset by callback
    short event; // reset by callback

    ModTrackerOnReadSession();
};

struct ModTrackerOnConnectSession
{
    tracker_on_connect_cbfunc_t cbfunc;
    ModTrackerSessionParam param;

    ModTrackerOnConnectSession();
    ModTrackerOnConnectSession(tracker_on_connect_cbfunc_t _cbfunc, ModTrackerSessionParam _param);
};

class ModTrackerClientBase
{
public:
    ModTrackerClientBase();

    virtual ~ModTrackerClientBase();

    // fill payload buffer of mt2t packet.
    // rb points to the end of mt2t header.
    // returns size of data, -1 for error.
    virtual int encode(const void* data, buffer* wb) = 0;

    // the implemented function will be call while the module related packet (response) is available.
    // rb points to the end of mt2t header of the coming packet.
    // buffer should point to the end of the packet after this function been called.
    // returns size of data, -1 for error.
    virtual int decode(buffer* rb) = 0;

    virtual void on_error(short which) { }
};

class ModTracker
{
public:
    static ModTracker& get_inst();
    static void destory_inst();

    int tracker_init(struct event_base * main_base, const ModTrackerConfig * config);
    void tracker_on_second(time_t t);
    void tracker_fini();

    ModTrackerConnection& get_alive_connection();
    bool close_alive_connection();
    bool connection_alive() const;

    const ModTrackerConfig* get_config() const { return _config; }

    bool send_request(const void* data, ModTrackerClientBase* client, int ver = VER_3);
    static void set_default_inbound_client(ModTrackerClientBase *client);

private:
    ModTracker(); // singleton
    ModTracker(const ModTracker&); // non-copyable

    tracker_on_read_cbfunc_t register_on_read_callback(tracker_on_read_cbfunc_t callback_func, TrackerModProto mp);

    // register_on_connect_callback is best to put after all other register
    tracker_on_connect_cbfunc_t register_on_connect_callback(tracker_on_connect_cbfunc_t callback_func, ModTrackerSessionParam param);

    tracker_on_error_cbfunc_t register_on_error_callback(tracker_on_error_cbfunc_t callback_func, module_t mod);

    void clearup_module_register(module_t mod = MT__FIRST);

    ModTrackerConnection& get_connection();

    f2t_v2_seq_t get_next_seq();

    bool _make_connection_alive();
    void _deal_with_seq_overflow();
    bool _init_connection();
    bool _reset_connection();
    size_t _check_timeout(time_t curr_time);
    size_t _check_timeout_client(time_t check_time);
    size_t _check_timeout_session(time_t check_time);
    static void _on_connect(connection_base* c);
    static void _on_read(connection_base* c, proto_t cmd);
    static void _on_error(connection_base* c, short which);

    bool _is_backup_ip_used;
    const ModTrackerConfig* _config;
    struct event_base* _main_base;
    ModTrackerConnection _tracker_conn;
    f2t_v2_seq_t _seq;
    time_t _last_check_session_timeout;

    typedef std::vector<ModTrackerOnConnectSession> on_conn_sess_vec_t;
    on_conn_sess_vec_t _cb_on_connect_session;

    typedef std::map<TrackerModProto, tracker_on_read_cbfunc_t> on_read_cbfunc_map_t;
    on_read_cbfunc_map_t _on_read_cbfunc_reg;

    typedef std::map<f2t_v2_seq_t, ModTrackerSessionParam> on_read_sess_map_t;
    on_read_sess_map_t _cb_on_read_session;

    typedef std::map<module_t, tracker_on_error_cbfunc_t> on_error_cbfunc_map_t;
    on_error_cbfunc_map_t _on_error_cbfunc_reg;

    typedef std::map<f2t_v2_seq_t, ModTrackerClientParam> on_read_cbobj_map_t;
    on_read_cbobj_map_t _on_read_cbobj_reg;
    static ModTrackerClientBase *_default_inbound_client;
};

#endif  /* MODULE_TRACKER_H_ */
