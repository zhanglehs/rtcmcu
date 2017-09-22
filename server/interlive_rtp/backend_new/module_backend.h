/*
 * interfaces of forward client and forward server
 * author: hechao@youku.com
 * create: 2013
 *
 */

#ifndef MODULE_BACKEND_
#define MODULE_BACKEND_
#include <stdint.h>
#include <event.h>
#include "util/session.h"
#include "cache_manager.h"
#include "common_defs.h"
#include "config_manager.h"
#include "rtp_backend_config.h"


#if (defined __cplusplus && !defined _WIN32)
//extern "C"
//{
#endif

class backend_config : public ConfigModule
{
private:
  bool inited;

public:
  char backend_listen_ip[32];
  uint16_t backend_listen_port;
  uint16_t backend_asn;
  uint16_t backend_region;
  uint16_t backend_level;
  uint16_t stream_timeout;
  uint32_t forward_client_capability;
  uint32_t forward_client_receive_buf_max_len;
  uint32_t forward_server_max_session_num;
  uint32_t fs_big_session_num;
  uint32_t fs_big_session_size;
  uint32_t fs_connection_buffer_size;

public:
  backend_config();
  virtual ~backend_config();
  backend_config& operator=(const backend_config& rhv);
  virtual void set_default_config();
  virtual bool load_config(xmlnode* xml_config);
  virtual bool reload() const;
  virtual const char* module_name() const;
  virtual void dump_config() const;

private:
  bool load_config_unreloadable(xmlnode* xml_config);
  bool load_config_reloadable(xmlnode* xml_config);
  bool resove_config();
};

typedef struct backend_state
{
  uint32_t streamid;
  uint32_t bps;
  uint32_t forward_cnt;
} backend_state;

void backend_on_second(time_t t);
int backend_init(struct event_base *mainbase,
struct session_manager *smng,
  const backend_config * backend_conf);

uint32_t backend_server_online_cnt(uint32_t streamid);

void backend_fini();
int backend_dump_state(uint32_t * total_used_out_bps,
  backend_state * states,
  uint32_t total_cnt,
  uint32_t * real_cnt);

void backend_on_millsecond(time_t t);

int32_t backend_start_stream_rtp(const StreamId_Ext& stream_id);
int32_t backend_stop_stream_rtp(const StreamId_Ext& stream_id);

void backend_del_stream_from_tracker_v3(StreamId_Ext stream_id, int level);

#if (defined __cplusplus && !defined _WIN32)
//}
#endif
#endif
