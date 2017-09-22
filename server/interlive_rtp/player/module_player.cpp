/**********************************************************
 * YueHonghui, 2013-05-02
 * hhyue@tudou.com
 * copyright:youku.com
 * ********************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "module_player.h"
#include "target.h"
#include "base_client.h"
#include "player.h"
//#include "rtp_udp_player.h"
#include "target_player.h"
#include "utils/memory.h"
#include "util/access.h"
#include "util/util.h"
#include "util/list.h"
#include "util/common.h"
#include "utils/buffer.hpp"
#include "util/flv.h"
#include "util/log.h"
#include "util/session.h"
#include "util/levent.h"
#include "util/report.h"
#include "testcase/delay_test.h"
#include "stream_manager.h"
#include "util/connection.h"
#include "hls.h"
//#include "http_server.h"

#include <assert.h>
#include <errno.h>
#include <event.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "../target_config.h"

using namespace std;

#define NOCACHE_HEADER "Expires: -1\r\nCache-Control: private, max-age=0\r\nPragma: no-cache\r\n"
#define HTTP_CONNECTION_HEADER "Connection: Close\r\n"
#define TS_UPPER (1000)
#define ENABLE_ADJUST_TS 0
//#define MAX_HTTP_REQ_LINE (4 * 1024)
#define MAX_BLOCK_PER_SECOND (13)
#define MAX_BLOCK_PER_8SECOND (80)
#define PLAYER_TOKEN_TIMEOUT (24*3600)
#define WRITE_MAX (64 * 1024)
#define WRITE_REMAIN_MAX (1024)
#define PLAYER_WRN(...) do{\
  WRN(__VA_ARGS__); \
  g_rpt_global.wrn_cnt++; \
  g_rpt_global.wrn_cnt_total++; \
}while (0);

#define PLAYER_ERR(...) do{\
  ERR(__VA_ARGS__); \
  g_rpt_global.err_cnt++; \
  g_rpt_global.err_cnt_total++; \
}while (0);

static player_config *g_conf = NULL;
static int player_listen_fd = -1;
static struct event ev_listener;
static struct event_base *g_main_base;
static session_manager *g_session_manager = NULL;
static const size_t write_step_min = 0;
static const size_t write_step_max = 32 * 1024;
static time_t g_time = 0;
static pid_t g_pid = 0;
static Player * g_player;


typedef enum
{
  LIVE_STREAM_START = 0,
  LIVE_STREAM_HEADER = 1,
  LIVE_STREAM_FIRST_TAG = 2,
  LIVE_STREAM_TAG = 3,
} live_stream_state;

typedef enum
{
  HLS_STREAM_START = 0,
  HLS_STREAM_FIRST_M3U8 = 1,
  HLS_STREAM_M3U8 = 2,
  HLS_STREAM_FIRST_TS = 3,
  HLS_STREAM_TS = 4,
} hls_stream_state;

typedef enum live_latency
{
  LIVE_AUDIO = 0,
  LIVE_IFRAME = 1,
  LIVE_PFRAME = 2,
  LIVE_NORMAL = 3,
} live_latency;

//char* g_latency_str[4] = {
//    "LIVE_AUDIO",
//    "LIVE_IFRAME",
//    "LIVE_PFRAME",
//    "LIVE_NORMAL",
//};

typedef enum
{
  PLAYER_WORK_START = 0,
  PLAYER_WORK_EATING = 1,
  PLAYER_WORK_HUNGERY = 2,
} player_work_state;

typedef enum
{
  LIVE_STUCK_AUDIO = 0,
  LIVE_STUCK_VIDEO = 1,
  LIVE_STUCK_NORMAL = 2,
}live_stuck;

typedef struct live_stream
{
  uint32_t streamid;

  uint32_t hungery_cnt;
  list_head_t hungery_players;

  uint32_t eating_cnt;
  list_head_t eating_players;
} live_stream;

typedef struct hls_stream
{
  uint32_t streamid;
  hls_ctx *ctx;

  uint32_t hungery_cnt;
  list_head_t hungery_players;

  uint32_t eating_cnt;
  list_head_t eating_players;

  time_t get_first_ts_time;
} hls_stream;

typedef struct live_conn
{
  connection *conn_base;
  uint32_t streamid;
  live_stream *stream_ptr;
  live_stream_state stream_state;
  player_work_state work_state;
  live_latency latency;
  live_stuck stuck_long;
  time_t check_point;
  uint64_t last_idx;
  int32_t delta_ts;
  int32_t last_audio_newts, last_video_newts;
  uint64_t last_block_seq;
  list_head_t head;
} live_conn;

typedef struct hls_conn
{
  connection *conn_base;
  uint32_t streamid;
  hls_stream *stream_ptr;
  hls_stream_state stream_state;
  player_work_state work_state;
  uint32_t next_idx;
  char* method;
  char* path;
  char* query_str;
  char* ua;
  list_head_t head;
} ts_conn;

struct player_stream
{
  live_stream *ls;
  hls_stream *hs;
  bool enable_hls;
};

typedef struct rpt_global
{
  uint32_t err_cnt;
  uint32_t wrn_cnt;
  uint32_t err_cnt_total;
  uint32_t wrn_cnt_total;
  uint32_t m3u8_srv_cnt;
  uint32_t ts_srv_cnt;
  uint32_t live_online_cnt;
} rpt_global;

static connection *g_conns_ptr = NULL;
static rpt_global g_rpt_global;
static void player_handler(const int fd, const short which, void *arg);

static void
player_handler(const int fd, const short which, void *arg)
{
  connection *c = (connection *)arg;

  assert(NULL != c);

  if (which & EV_READ) {
    int r = 0;
    char method[32];
    char path[128];
    char query[1024];
    //char streamid_buf[32];
    //int64_t streamid = 0;

    if (PLAYER_NA == c->bind_flag) {
      // GET /v1/get_stream?streamid=X&programid=[live/m3u8/ts] HTTP/1.X
      //int len = 0;

      r = buffer_read_fd_max(c->r, fd, MAX_HTTP_REQ_LINE);

      if (r < 0)
        conn_close(c);
      if (r <= 0)
        return;

      const char *lrcf2 = (const char *)memmem(buffer_data_ptr(c->r),
        buffer_data_len(c->r), "\r\n\r\n", strlen("\r\n\r\n"));

      if (NULL == lrcf2 || lrcf2 == buffer_data_ptr(c->r)) {
        if (buffer_data_len(c->r) >= MAX_HTTP_REQ_LINE) {
          PLAYER_WRN("http req line too long. #%d(%s:%hu), len = %zu",
            c->fd, c->remote_ip, c->remote_port,
            buffer_data_len(c->r));
          ACCESS("player %s:%6hu 414", c->remote_ip,
            c->remote_port);
          util_http_rsp_error_code(c->fd, HTTP_414);
          conn_close(c);
        }
        return;
      }

      int lrcf2_l = lrcf2 - (char *)buffer_data_ptr(c->r);
      const char *lrcf = (const char *)memmem(buffer_data_ptr(c->r),
        lrcf2_l + 4, "\r\n", strlen("\r\n"));
      int lrcf_l = lrcf - (char *)buffer_data_ptr(c->r);

      int parse_req = util_http_parse_req_line(buffer_data_ptr(c->r), lrcf_l,
        method, sizeof(method),
        path, sizeof(path),
        query, sizeof(query));
      if (-1 == parse_req) {
        WRN("parse_req_line failed. connection closing. #%d(%s:%hu)",
          c->fd, c->remote_ip, c->remote_port);
        ACCESS("player %s:%6hu 400", c->remote_ip, c->remote_port);
        util_http_rsp_error_code(c->fd, HTTP_400);
        conn_close(c);
        return;
      }

      if (strlen(method) != 3 || 0 != strncmp(method, "GET", 3)) {
        WRN("method not valid. connection closing. #%d(%s:%hu), method = %s", c->fd, c->remote_ip, c->remote_port, method);
        ACCESS("player %s:%6hu %s %s?%s 404", c->remote_ip,
          c->remote_port, method, path, query);
        util_http_rsp_error_code(c->fd, HTTP_404);
        conn_close(c);
        return;
      }
#define CROSSDOMAIN "/crossdomain.xml"
      if (strlen(CROSSDOMAIN) == strlen(path) && 0 == strncmp(path, CROSSDOMAIN, strlen(CROSSDOMAIN)) && 0 == parse_req)
      {
        char rsp[1024];
        int used = snprintf(rsp, sizeof(rsp),
          "HTTP/1.0 200 OK\r\nServer: Youku Live Forward\r\n"
          "Content-Type: application/xml;charset=utf-8\r\n"
          "Host: %s:%d\r\n"
          NOCACHE_HEADER
          HTTP_CONNECTION_HEADER
          "Content-Length: %zu\r\n\r\n",
          c->local_ip, c->local_port, g_conf->crossdomain_len);

        if (0 != buffer_expand_capacity(c->w, g_conf->crossdomain_len + used)) {
          PLAYER_ERR("player-ts buffer_expand_capacity failed. len = %zu",
            used + g_conf->crossdomain_len);
          ACCESS("crossdomain %s:%6hu GET /crossdomain.xml 500",
            c->remote_ip, c->remote_port);
          util_http_rsp_error_code(c->fd, HTTP_500);
          conn_close(c);
          return;
        }
        buffer_append_ptr(c->w, rsp, used);
        buffer_append_ptr(c->w, g_conf->crossdomain, g_conf->crossdomain_len);
        conn_enable_write(c, g_main_base, player_handler);
        ACCESS("crossdomain %s:%6hu GET /crossdomain.xml 200",
          c->remote_ip, c->remote_port);
        c->bind_flag = PLAYER_CROSSDOMAIN;
        return;
      }
      else
      {
        if (1 != parse_req)
        {
          WRN("parse_req_line failed. connection closing. #%d(%s:%hu)",
            c->fd, c->remote_ip, c->remote_port);
          ACCESS("player %s:%6hu 400", c->remote_ip, c->remote_port);
          util_http_rsp_error_code(c->fd, HTTP_400);
          conn_close(c);
          return;
        }

        // new url format which will replace /v1/ and /v2/ in the future,
        // now keep /v1/ and /v2/ for compatibility
        if (0 == strncmp(path, "/download/sdp/", strlen("/download/sdp/"))
          || 0 == strncmp(path, "/live/nodelay/v1/", strlen("/live/nodelay/v1/"))
          )
        {
          c->bind_flag = PLAYER_V2_FRAGMENT;
          DBG("v2 fragment player is working !");
          g_player->add_play_task(c, lrcf2, method, path, query);
          return;
        }
      }
    }
    else
    {
      char buf[128];

      r = read(c->fd, buf, 128);
      if ((r == -1 && errno != EINTR && errno != EAGAIN) || r == 0)
      {
        switch (c->bind_flag) {
        case PLAYER_CROSSDOMAIN:
        {
          INF("crossdomain #%d(%s:%hu) disconnected...",
            c->fd, c->remote_ip, c->remote_port);
          conn_close(c);
          return;
        }
        default:
          PLAYER_ERR("unexpected connection bind_flag = %d. #%d(%s:%hu)",
            c->bind_flag, c->fd, c->remote_ip, c->remote_port);
          assert(0);
          return;
        }
      }
    }
  }
  else if (which & EV_WRITE) {
    switch (c->bind_flag) {
    case PLAYER_NA:
      conn_disable_write(c, g_main_base, player_handler);
      return;
    case PLAYER_CROSSDOMAIN:
    {
      if (-1 == buffer_write_fd_max(c->w, c->fd, WRITE_MAX)) {
        INF("crossdomain #%d(%s:%hu) disconnected...",
          c->fd, c->remote_ip, c->remote_port);
        conn_close(c);
        return;
      }
      if (0 == buffer_data_len(c->w))
      {
        conn_disable_write(c, g_main_base, player_handler);
        conn_close(c);
      }
      return;
    }
    default:
      PLAYER_ERR("unexpected connection bind_flag. %d", c->bind_flag);
      assert(0);
    }
  }
}

static int
set_player_sockopt(int fd)
{
  int ret;

  util_set_cloexec(fd, TRUE);
  ret = util_set_nonblock(fd, TRUE);
  if (0 != ret) {
    PLAYER_ERR("set nonblock failed. ret = %d, fd = %d, error = %s",
      ret, fd, strerror(errno));
    return -1;
  }

  int val = 1;

  if (-1 == setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
    (void *)&val, sizeof(val))) {
    PLAYER_ERR("set tcp nodelay failed. fd = %d, error = %s",
      fd, strerror(errno));
    return -2;
  }

  if (-1 == setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
    (void *)&g_conf->sock_snd_buf_size,
    sizeof(g_conf->sock_snd_buf_size))) {
    PLAYER_ERR("set player sock snd buf sz failed. fd = %d, error = %s", fd,
      strerror(errno));
    return -3;
  }
  return 0;
}

static void
player_accept(const int fd, const short which, void *arg)
{
  TRC("fd = %d", fd);
  connection *c = NULL;
  int newfd = -1;
  struct sockaddr_in s_in;
  socklen_t len = sizeof(struct sockaddr_in);

  memset(&s_in, 0, len);
  newfd = accept(fd, (struct sockaddr *) &s_in, &len);
  if (-1 == newfd) {
    PLAYER_ERR("fd #%d accept() failed, error = %s", fd, strerror(errno));
    return;
  }

  if (newfd >= g_conf->max_players) {
    PLAYER_WRN("fd too big, fd = %d, max_players = %d",
      newfd, g_conf->max_players);
    util_http_rsp_error_code(newfd, HTTP_503);
    ACCESS("player %s:%6hu 503", util_ip2str_no(s_in.sin_addr.s_addr),
      ntohs(s_in.sin_port));
    close(newfd);
    return;
  }
  if (0 != set_player_sockopt(newfd)) {
    PLAYER_WRN("set newfd sockopt failed. fd = %d", newfd);
    util_http_rsp_error_code(newfd, HTTP_500);
    ACCESS("player %s:%6hu 500", util_ip2str_no(s_in.sin_addr.s_addr),
      ntohs(s_in.sin_port));
    close(newfd);
    return;
  }
  c = g_conns_ptr + newfd;
  assert(-1 == c->fd);
  if (0 !=
    conn_init(c, newfd, g_conf->buffer_max, g_main_base, player_handler,
    g_session_manager, 10, PLAYER_NA)) {
    PLAYER_ERR("connection init failed. fd = %d", newfd);
    util_http_rsp_error_code(newfd, HTTP_500);
    ACCESS("player %s:%6hu 500", util_ip2str_no(s_in.sin_addr.s_addr),
      ntohs(s_in.sin_port));
    close(newfd);
    return;
  }

  INF("player connected #%d(%s:%hu)", c->fd, c->remote_ip, c->remote_port);
}

// INFO: zhangle, flv live play url like this:
// http://192.168.245.133:8089/live/nodelay/v1/00000000000000000000000015958F05?token=98765
int
player_init(struct event_base *main_base, struct session_manager *smng,
const player_config * config)
{
  if (NULL == main_base || NULL == config) {
    PLAYER_ERR("main_base or config is NULL. main_base = %p, config = %p",
      main_base, config);
    return -1;
  }
  g_main_base = main_base;
  g_session_manager = smng;
  g_time = time(NULL);
  g_conf = (player_config *)config;
  memset(&g_rpt_global, 0, sizeof(g_rpt_global));
  int i = 0;
  int val = 0, ret = 0;
  struct sockaddr_in addr;

  player_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == player_listen_fd) {
    PLAYER_ERR("socket failed. error = %s", strerror(errno));
    return -2;
  }

  val = 1;
  if (-1 == setsockopt(player_listen_fd, SOL_SOCKET, SO_REUSEADDR,
    (void *)&val, sizeof(val))) {
    PLAYER_ERR("set socket SO_REUSEADDR failed. error = %s", strerror(errno));
    return -5;
  }

  if (0 != set_player_sockopt(player_listen_fd)) {
    PLAYER_ERR("set player_listen_fd sockopt failed.");
    return -6;
  }
#ifdef TCP_DEFER_ACCEPT
  {
    int v = 30;             /* 30 seconds */

    if (-1 ==
      setsockopt(player_listen_fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &v,
      sizeof(v))) {
      PLAYER_WRN("can't set TCP_DEFER_ACCEPT on player_listen_fd %d",
        player_listen_fd);
    }
  }
#endif

  addr.sin_family = AF_INET;
  inet_aton("0.0.0.0", &(addr.sin_addr));
  addr.sin_port = htons(g_conf->player_listen_port);

  ret = bind(player_listen_fd, (struct sockaddr *) &addr, sizeof(addr));
  if (0 != ret) {
    PLAYER_ERR("bind addr failed. error = %s", strerror(errno));
    return -8;
  }

  ret = listen(player_listen_fd, 5120);
  if (0 != ret) {
    PLAYER_ERR("listen player fd failed. error = %s", strerror(errno));
    return -9;
  }

  g_conns_ptr =
    (connection *)mcalloc(g_conf->max_players, sizeof(connection));
  if (NULL == g_conns_ptr) {
    PLAYER_ERR("g_conns mcalloc failed.");
    close(player_listen_fd);
    return -10;
  }

  for (i = 0; i < g_conf->max_players; i++) {
    g_conns_ptr[i].fd = -1;
  }

  g_pid = getpid();
  srand(g_pid);    /* init random seed */

  levent_set(&ev_listener, player_listen_fd, EV_READ | EV_PERSIST,
    player_accept, NULL);
  event_base_set(g_main_base, &ev_listener);
  levent_add(&ev_listener, 0);
  //http_server_add_handler("/module_player_state", NULL, mp_get_state);

  g_player = new Player(main_base, smng, config);


  //  TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");
  //  if (common_config->enable_rtp)
  //  {
  //RTPUDPPlayer::getInst()->init(main_base);
  //  }
  return 0;
}

void
get_sdp_cb(StreamId_Ext sid, string &sdp)
{
  // TODO: zhangle
  //sdp = RTPUDPPlayer::getInst()->get_sdp(sid);
}

void
player_fini()
{
  // TODO: zhangle
  //   if (NULL != g_player)
  //   {
  //       delete g_player;
  //       g_player = NULL;
  //   }
  //   
  //RTPUDPPlayer::destroyInst();

  //   if (NULL != g_conns_ptr)
  //   {
  //       mfree(g_conns_ptr);
  //       g_conns_ptr = NULL;
  //   }

  //   levent_del(&ev_listener);
  //   close(player_listen_fd);
  //   player_listen_fd = 0;
}

Player* get_player_inst()
{
  return g_player;
}
player_config::player_config()
{
  inited = false;
  set_default_config();
}

player_config::~player_config()
{
}

player_config& player_config::operator=(const player_config& rhv)
{
  memmove(this, &rhv, sizeof(player_config));
  return *this;
}

void player_config::set_default_config()
{
  player_listen_ip[0] = 0;
  memset(player_listen_ip, 0, sizeof(player_listen_ip));
  player_listen_port = 10001;
  timeout_second = 60;
  stuck_long_duration = 5 * 60;
  max_players = 8000;
  always_generate_hls = false;
  ts_target_duration = 8;
  ts_segment_cnt = 4;
  buffer_max = 2 * 1024 * 1024;
  sock_snd_buf_size = 32 * 1024;
  normal_offset = 0;
  latency_offset = 3;
  memset(private_key, 0, sizeof(private_key));
  memset(super_key, 0, sizeof(super_key));
  memset(crossdomain, 0, sizeof(crossdomain));
  crossdomain_len = 0;
}

bool player_config::load_config(xmlnode* xml_config)
{
  ASSERTR(xml_config != NULL, false);
  xmlnode *p = xmlgetchild(xml_config, "player", 0);
  ASSERTR(p != NULL, false);

  return load_config_unreloadable(p) && load_config_reloadable(p) && resove_config();
}

bool player_config::reload() const
{
  return true;
}

const char* player_config::module_name() const
{
  return "player";
}

void player_config::dump_config() const
{
  INF("player config: "
    "player_listen_ip=%s, player_listen_port=%u, "
    "stuck_long_duration=%d, timeout_second=%d, max_players=%d, buffer_max=%d, "
    "always_generate_hls=%d, ts_target_duration=%u, ts_segment_cnt=%u, "
    "private_key=%s, super_key=%s, sock_snd_buf_size=%d, normal_offset=%d, latency_offset=%d, "
    "crossdomain_path=%s, crossdomain_len=%lu",
    player_listen_ip, uint32_t(player_listen_port),
    stuck_long_duration, timeout_second, max_players, (int)buffer_max,
    int(always_generate_hls), ts_target_duration, ts_segment_cnt,
    private_key, super_key, sock_snd_buf_size, normal_offset, latency_offset,
    crossdomain_path.c_str(), crossdomain_len);
}

bool player_config::load_config_unreloadable(xmlnode* xml_config)
{
  ASSERTR(xml_config != NULL, false);
  if (inited)
    return true;

  char *q = NULL;
  int tm = 0;

  q = xmlgetattr(xml_config, "listen_host");
  if (NULL != q)
  {
    strncpy(player_listen_ip, q, 31);
  }

  q = xmlgetattr(xml_config, "listen_port");
  if (!q)
  {
    fprintf(stderr, "player_listen_port get failed.\n");
    return false;
  }
  player_listen_port = atoi(q);
  if (player_listen_port <= 0)
  {
    fprintf(stderr, "player_listen_port not valid.\n");
    return false;
  }

  q = xmlgetattr(xml_config, "socket_send_buffer_size");
  if (!q)
  {
    fprintf(stderr, "socket_send_buffer_size get failed.\n");
    return false;
  }
  tm = atoi(q);
  if (tm <= 6 * 1024 || tm > 512 * 1024 * 1024)
  {
    fprintf(stderr, "socket_send_buffer_size not valid. value = %d\n", tm);
    return false;
  }
  sock_snd_buf_size = tm;

  q = xmlgetattr(xml_config, "buffer_max");
  if (!q)
  {
    fprintf(stderr, "player buffer_max get failed.\n");
    return false;
  }
  tm = atoi(q);
  if (tm <= 0 || tm >= 20 * 1024 * 1024)
  {
    fprintf(stderr, "player buffer_max not valid. value = %d\n", tm);
    return false;
  }
  buffer_max = tm;

  q = xmlgetattr(xml_config, "max_players");
  if (!q)
  {
    fprintf(stderr, "max_players get failed.\n");
    return false;
  }
  max_players = atoi(q);
  if (max_players <= 0)
  {
    fprintf(stderr, "max_players not valid.\n");
    return false;
  }

  q = xmlgetattr(xml_config, "always_generate_hls");
  if (!q)
  {
    fprintf(stderr, "parse always_generate_hls failed.");
    return false;
  }
  if (strcasecmp(q, "true") == 0)
    always_generate_hls = true;
  else
    always_generate_hls = false;

  q = xmlgetattr(xml_config, "ts_target_duration");
  if (!q || !util_check_digit(q, strlen(q)))
  {
    fprintf(stderr, "ts_target_duration get failed.\n");
    return false;
  }
  ts_target_duration = atoi(q);
  if (ts_target_duration <= 0)
  {
    fprintf(stderr, "ts_target_duration not valid.\n");
    return false;
  }

  q = xmlgetattr(xml_config, "ts_segment_cnt");
  if (!q || !util_check_digit(q, strlen(q)))
  {
    fprintf(stderr, "ts_segment_cnt get failed.\n");
    return false;
  }
  ts_segment_cnt = atoi(q);
  if (ts_segment_cnt <= 0)
  {
    fprintf(stderr, "ts_segment_cnt not valid.\n");
    return false;
  }

  inited = true;
  return true;
}

bool player_config::load_config_reloadable(xmlnode* xml_config)
{
  ASSERTR(xml_config != NULL, false);
  char *q = NULL;

  q = xmlgetattr(xml_config, "private_key");
  if (!q)
  {
    fprintf(stderr, "player private_key get failed.\n");
    return false;
  }
  strncpy(private_key, q, sizeof(private_key)-1);

  q = xmlgetattr(xml_config, "super_key");
  if (!q)
  {
    fprintf(stderr, "player super_key get failed.\n");
    return false;
  }
  strncpy(super_key, q, sizeof(super_key)-1);

  q = xmlgetattr(xml_config, "timeout_second");
  if (!q)
  {
    fprintf(stderr, "timeout_second get failed.\n");
    return false;
  }
  timeout_second = atoi(q);
  if (timeout_second <= 0)
  {
    fprintf(stderr, "timeout_second not valid.\n");
    return false;
  }

  q = xmlgetattr(xml_config, "normal_offset");
  if (!q)
  {
    fprintf(stderr, "normal_offset get failed.\n");
    return false;
  }
  normal_offset = atoi(q);
  if (normal_offset < 0)
  {
    fprintf(stderr, "normal_offset %d not valid.\n", normal_offset);
    return false;
  }

  q = xmlgetattr(xml_config, "latency_offset");
  if (!q)
  {
    fprintf(stderr, "latency_offset get failed.\n");
    return false;
  }
  latency_offset = atoi(q);
  if (latency_offset < 0)
  {
    fprintf(stderr, "latency_offset %d not valid.\n", latency_offset);
    return false;
  }

  q = xmlgetattr(xml_config, "stuck_long_duration");
  if (!q)
  {
    fprintf(stderr, "stuck_long_duration get failed.\n");
    return false;
  }
  stuck_long_duration = atoi(q);
  if (stuck_long_duration < 0)
  {
    fprintf(stderr, "stuck_long_duration %d not valid.\n", stuck_long_duration);
    return false;
  }

  q = xmlgetattr(xml_config, "crossdomain_path");
  if (!q)
  {
    fprintf(stderr, "crossdomain_path get failed.\n");
    return false;
  }

  if (q[0] == '/')
  {
    crossdomain_path = q;
  }
  else
  {
    TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");
    //common_config->config_file.dir;
    crossdomain_path = common_config->config_file.dir + string(q);
  }

  if (!parse_crossdomain_config())
  {
    fprintf(stderr, "parse_crossdomain_config failed.\n");
    return false;
  }

  return true;
}

bool player_config::resove_config()
{
  char ip[32] = { '\0' };
  int ret;

  if (strlen(player_listen_ip) == 0)
  {
    if (g_public_cnt < 1)
    {
      fprintf(stderr, "player listen ip not in conf and no public ip found\n");
      return false;
    }
    strncpy(player_listen_ip, g_public_ip[0], sizeof(player_listen_ip)-1);
  }

  strncpy(ip, player_listen_ip, sizeof(ip)-1);
  ret = util_interface2ip(ip, player_listen_ip, sizeof(player_listen_ip)-1);
  if (0 != ret)
  {
    fprintf(stderr, "player_listen_host resolv failed. host = %s, ret = %d\n", player_listen_ip, ret);
    return false;
  }

  return true;
}

bool player_config::parse_crossdomain_config()
{
  int f = open(crossdomain_path.c_str(), O_RDONLY);
  if (-1 == f)
  {
    fprintf(stderr, "crossdomain file not valid. path = %s, error = %s \n", crossdomain_path.c_str(), strerror(errno));
    return false;
  }

  ssize_t total = 0;
  ssize_t n = 0;
  ssize_t max = sizeof(crossdomain);
  while (total < (int)(max - 1))
  {
    n = read(f, crossdomain + total, max - total);
    if (-1 == n)
    {
      fprintf(stderr, "crossdomain read failed. error = %s\n", strerror(errno));
      goto fail;
    }
    if (0 == n)
    {
      goto success;
    }
    if (n + total >= (int)max)
    {
      fprintf(stderr, "crossdomain too long. max = %zu\n", max - 1);
      goto fail;
    }
    total += n;
  }
fail:
  close(f);
  return false;

success:
  close(f);
  crossdomain[max - 1] = 0;
  crossdomain_len = strlen(crossdomain);
  return true;
}
