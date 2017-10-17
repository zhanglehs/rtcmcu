/*******************************************
 * YueHonghui, 2013-06-05
 * hhyue@tudou.com
 * copyright:youku.com
 * ****************************************/
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <event.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>
#include <pwd.h>

#include <appframe/singleton.hpp>
#include <common/type_defs.h>
#include "target_player.h"
#include "player/module_player.h"
#include "player/rtp_player_config.h"

#include "uploader/uploader_config.h"
#include "uploader/RtpTcpConnectionManager.h"
#include "uploader/rtp_uploader_config.h"

#include "target_backend.h"
#include "module_tracker.h"
#include "config_manager.h"

#include "util/access.h"
#include "util/common.h"
#include "util/log.h"
#include "util/xml.h"
#include "util/hashtable.h"
#include "utils/memory.h"
#include "util/util.h"
#include "util/session.h"
#include "util/backtrace.h"
#include "util/levent.h"
#include "util/report.h"
#include "util/report_stats.h"
#include "streamid.h"
#include "common/proto.h"
#include "define.h"
#include "cache_manager.h"
#include "portal.h"
#include "perf.h"
#include "info_collector.h"

#include "network/base_http_server.h"
#include "whitelist_manager.h"
#include "config.h"
#include "target_config.h"
#include "player/module_player.h"

#include "evhttp.h"

//#define gperf
#ifdef gperf
#include <google/heap-profiler.h>
#endif

#include "event_loop.h"
#include "backend_new/module_backend.h"
#include "backend_new/rtp_backend_config.h"
using namespace lcdn::net;

#include "publisher/flv_publisher.h"

using namespace std;
using namespace fragment;
using namespace media_manager;

INIT_SINGLETON(WhitelistManager);

INIT_SINGLETON(config);
static config &g_conf = *SINGLETON(config);
static char g_configfile[PATH_MAX];

static struct event_base *main_base = NULL;
static struct event g_ev_timer;

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_reload_config = 0;
char g_public_ip[1][32];
int  g_public_cnt;
char g_private_ip[1][32];
int  g_private_cnt;
char g_ver_str[64];

static Perf *perf = NULL;

static void init_config_manager(ConfigManager& config_manager, config& conf) {
  config_manager.set_config_module(&(conf.target_conf));
  config_manager.set_config_module(&(conf.player));
  config_manager.set_config_module(&(conf.rtp_player_config));
  config_manager.set_config_module(&(conf.uploader));
  config_manager.set_config_module(&(conf.rtp_uploader_config));
  config_manager.set_config_module(&(conf.backend));
  config_manager.set_config_module(&(conf.rtp_backend_config));
  config_manager.set_config_module(&(conf.tracker));
  config_manager.set_config_module(&(conf.cache_manager_config));
  //config_manager.set_config_module(&(conf.http_config));
  config_manager.set_config_module(&(conf.fcrtp_config));
  config_manager.set_config_module(&(conf.http_ser_config));
  config_manager.set_config_module(&(conf.publisher_config));

  config_manager.set_default_config();
}

static int resolv_ip_addr() {
  int ret = 0;
  //ret = request_public_ip();
  //ret = util_get_ips((char **)g_public_ip, 1, 32, &g_public_cnt, util_ipfilter_public);
  if (0 != ret || g_public_cnt < 1)
  {
    if (ret > 0)
      fprintf(stderr, "warning...get public ip failed. error = %s\n", strerror(errno));
    else
      fprintf(stderr, "warning...get public ip failed. ret = %d, cnt = %d\n", ret, g_public_cnt);
  }

  ret = util_get_ips((char **)g_private_ip, 1, 32, &g_private_cnt, util_ipfilter_private);
  if (0 != ret || g_private_cnt < 1)
  {
    if (ret > 0)
      fprintf(stderr, "warning...get private ip failed. error = %s\n", strerror(errno));
    else
      fprintf(stderr, "warning...get private ip failed. ret = %d, cnt = %d\n", ret, g_private_cnt);
  }

  return ret;
}

static int parse_config_file(ConfigManager& config_manager, char *file)
{
  ASSERTR(file != NULL, -1);

  if (0 != resolv_ip_addr())
  {
    ERR("resolv ip address failed.");
    return -1;
  }

  char config_full_path[PATH_MAX];
  if (NULL == realpath(file, config_full_path))
  {
    fprintf(stderr, "resolve config file path failed. \n");
    return -1;
  }

  xmlnode* mainnode = xmlloadfile(file);
  if (mainnode == NULL)
  {
    fprintf(stderr, "mainnode get failed. \n");
    return -1;
  }

  char binary_full_path[PATH_MAX];
  int cnt = readlink("/proc/self/exe", binary_full_path, PATH_MAX);
  if (cnt < 0 || cnt >= PATH_MAX)
  {
    fprintf(stderr, "resolve binary file path failed. \n");
    return -1;
  }

  TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");
  common_config->config_file.load_full_path(config_full_path);
  common_config->binary_file.load_full_path(binary_full_path);

  struct xmlnode *root = xmlgetchild(mainnode, "interlive", 0);
  if (root == NULL)
  {
    fprintf(stderr, "node interlive get failed. \n");
    freexml(mainnode);
    return -1;
  }

  if (config_manager.load_config(root))
  {
    freexml(mainnode);
    return 0;
  }
  else
  {
    fprintf(stderr, "parse_config_file failed.\n");
    freexml(mainnode);
    return -1;
  }
}

static void show_help(void) {
  fprintf(stderr, "Interlive %s %s, Build-Date: " __DATE__ " " __TIME__ "\n"
    "Usage:\n" " -h this message\n"
    " -c file, config file, default is %s.xml\n"
    " -r reload config file\n" " -D don't go to background\n"
    " -v verbose\n\n", PROCESS_NAME, g_ver_str, PROCESS_NAME);
}

static void server_exit()
{
  INF("hash destroy...");
  SINGLETON(WhitelistManager)->clear();
  INF("player fini...");
  //player_fini();
  LiveConnectionManager::DestroyInstance();
  INF("uploader fini...");
  //RtpTcpConnectionManager::destroy_inst();
  //RtpUdpConnectionManager::destroy_inst();
  RtpUdpServerManager::DestroyInstance();
  RtpTcpServerManager::DestroyInstance();
  INF("cache manager fini...");
  FlvCacheManager::DestroyInstance();
  INF("backend fini...");
  backend_fini();
  INF("access fini...");
  access_fini();
  //INF("stream manager fini...");
  //stream_manager_fini();
  INF("portal fini...");
  Portal::destroy();
  INF("tracker fini...");
  ModTracker::get_inst().tracker_fini();
  ModTracker::destory_inst();
  INF("Perf fini...");
  Perf::destory();
  INF("http server destroy...");
  //http_server_fini();
  INF("begin to backtrace fini...");
  backtrace_fini();
  report_fini();
  INF("program stopping...");
  log_fini();
  levent_fini();
  event_base_free(main_base);
}

static void reload_config_sig_handler(int sig) {
  SIG(sig);
  g_reload_config = TRUE;
  return;
}

static void exit_sig_handler(int sig) {
#ifdef gperf
  HeapProfilerStop();
  printf("HeapProfilerStop\n");
#endif
  SIG(sig);
  g_stop = TRUE;
  return;
}

static void sec_timer_service() {
  TRC("sec_timer_service");
  if (g_stop) {
    DBG("sec_timer_service: begin to exit...");
    server_exit();
    exit(0);
  }

  if (g_reload_config) {
    g_reload_config = FALSE;
    DBG("sec_timer_service: begin to reload config...");

    ConfigManager& config_manager(ConfigManager::get_inst());

    INF("dump old config...");
    config_manager.dump_config();

    int ret = parse_config_file(config_manager, g_configfile);
    if (ret != 0) {
      ERR("sec_timer_service: reload config failed, ret = %d", ret);
      return;
    }

    config_manager.reload();

    INF("sec_timer_service: dump reloaded config...");
    config_manager.dump_config();
  }

  if (time(NULL) % 3 == 0) {
    perf->keep_update();
  }
}

static void timer_service(const int fd, short which, void *arg) {
  static int last_timer = time(NULL);
  time_t now = time(NULL);
  if (now - last_timer >= 1) {
    sec_timer_service();
    last_timer = now;
  }
  publisher_on_millsecond(now);

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 10000;
  evtimer_set(&g_ev_timer, timer_service, NULL);
  event_base_set(main_base, &g_ev_timer);
  evtimer_add(&g_ev_timer, &tv);
}

static int get_pid(pid_t * pid) {
  assert(NULL != pid);

  TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");

  char filename[PATH_MAX];
  memset(filename, 0, sizeof(filename));
  snprintf(filename, sizeof(filename)-1, "%s.pid", PROCESS_NAME);

  string pidfile = common_config->config_file.dir + string(filename);

  int fd = open(pidfile.c_str(), O_EXCL | O_RDONLY);

  if (-1 == fd) {
    fprintf(stderr, "open pid file failed. path = %s, error = %s\n",
      pidfile.c_str(), strerror(errno));
    return -1;
  }
  char pid_char[PATH_MAX];
  int len = read(fd, pid_char, 256);

  if (-1 == len) {
    fprintf(stderr, "read failed. error = %s\n", strerror(errno));
    close(fd);
    return -2;
  }
  char *endptr = NULL;
  long p = strtol(pid_char, &endptr, 10);

  if (LONG_MIN == p || LONG_MAX == p) {
    close(fd);
    return -3;
  }
  if (p != (int)p) {
    close(fd);
    return -4;
  }
  *pid = p;
  close(fd);
  return 0;
}

static void signal_to_reload()
{
  pid_t pid = 0;
  int ret = get_pid(&pid);

  if (0 != ret) {
    fprintf(stderr, "get pid failed. reload failed. ret = %d\n", ret);
    return;
  }
  ret = kill(pid, SIGUSR1);
  if (0 != ret) {
    fprintf(stderr, "kill failed. error = %s\n", strerror(errno));
    return;
  }
}

static int main_proc() {
  ConfigManager& config_manager(ConfigManager::get_inst());
  g_conf.cache_manager_config.init(true);
  init_config_manager(config_manager, g_conf);
  int ret = parse_config_file(config_manager, g_configfile);
  if (0 != ret) {
    ERR("parse xml %s failed, ret = %d", g_configfile, ret);
    return 1;
  }

  char* host_ip = NULL;
  host_ip = g_public_cnt > 0 ? (char*)g_public_ip[0] : (g_private_cnt > 0 ? (char*)g_private_ip[0] : NULL);
  if (NULL == host_ip) {
    fprintf(stderr, "no public or private ip found.\n");
    return 1;
  }

  if (LOGGER_FILE == g_conf.target_conf.logger_mode) {
    char mkcmd[PATH_MAX] = "logs/";
    sprintf(mkcmd, "mkdir -p %s", g_conf.target_conf.logdir);
    system(mkcmd);
    chmod(g_conf.target_conf.logdir, 0777);

    ret = log_init_file(g_conf.target_conf.logdir, PROCESS_NAME, LOG_LEVEL_DBG, 300 * 1024 * 1024);
    if (0 != ret) {
      fprintf(stderr, "logdir = %s, log init file failed. ret = %d\n",
        g_conf.target_conf.logdir, ret);
      return 1;
    }
    char accesspath[PATH_MAX];

    accesspath[PATH_MAX - 1] = 0;
    snprintf(accesspath, PATH_MAX - 1, "%s_access", PROCESS_NAME);
    ret = access_init_file(g_conf.target_conf.logdir, accesspath, 300 * 1024 * 1024);
    if (0 != ret)
    {
      fprintf(stderr, "access init failed. logdir = %s, ret = %d\n",
        g_conf.target_conf.logdir, ret);
      return 1;
    }
  }
  else if (LOGGER_NET == g_conf.target_conf.logger_mode) {
    ret = log_init_net(g_conf.target_conf.remote_logger_path, PROCESS_NAME, host_ip,
      g_conf.backend.backend_listen_port, LOG_LEVEL_DBG);
    if (0 != ret) {
      fprintf(stderr,
        "log_remote_path = %s, log init net failed. ret = %d\n",
        g_conf.target_conf.remote_logger_path, ret);
      return 1;
    }
    ret = access_init_net(g_conf.target_conf.remote_logger_path, PROCESS_NAME, host_ip,
      g_conf.backend.backend_listen_port);
    if (0 != ret) {
      fprintf(stderr,
        "remote_logger_path = %s, access_init net failed. ret = %d\n",
        g_conf.target_conf.remote_logger_path, ret);
      return 1;
    }
  }
  else {
    snprintf(g_log_ctx.prefix_tags, sizeof(g_log_ctx.prefix_tags) - 1,
      "%s:%hu\tLOG\t%s", host_ip, g_conf.backend.backend_listen_port, PROCESS_NAME);
    snprintf(g_conf_access.prefix_tags, sizeof(g_conf_access.prefix_tags) - 1,
      "%s:%hu\tACCESS\t%s", host_ip, g_conf.backend.backend_listen_port, PROCESS_NAME);
  }

  log_set_level(g_conf.target_conf.log_level);
  log_set_max_size(g_conf.target_conf.log_cut_size_MB * 1024 * 1024);
  access_set_max_size(g_conf.target_conf.access_cut_size_MB * 1024 * 1024);

  INF("on boot.after resolv...");
  config_manager.dump_config();

  // TODO hechao
  /*
  if(0 != util_set_limit(RLIMIT_NOFILE, 40000, 40000)) {
  ERR("set file no limit to 40000 failed.");
  return 1;
  }
  */
#ifdef HAVE_SCHED_GETAFFINITY
  if (0 != util_unmask_cpu0()) {
    ERR("unmask cpu0 failed.");
    return 1;
  }
#endif

  uint8_t module_type = MODULE_TYPE_BACKEND;
  if (g_conf.target_conf.enable_uploader) {
    module_type = MODULE_TYPE_UPLOADER;
  }

  FlvCacheManager::Instance()->Init(main_base, module_type, &(g_conf.cache_manager_config));

  /* ignore SIGPIPE when write to closing fd
   * otherwise EPIPE error will cause transporter to terminate unexpectedly
   */
  signal(SIGPIPE, SIG_IGN);

  HttpServerManager::Instance()->Init(main_base, g_conf.http_ser_config.listen_port);
  //FlvCacheManager::Instance()->set_http_server(base_http_server);

  perf = Perf::get_instance();
  perf->set_cpu_rate_threshold(g_conf.target_conf.cpu_rate_threshold);

  RtpTcpServerManager::Instance()->Init(main_base);
  RtpUdpServerManager::Instance()->Init(main_base);

  if (0 != LiveConnectionManager::Instance()->Init(main_base, &(g_conf.player))) {
    ERR("player init failed.");
    return 1;
  }

  // INFO: zhangle, flv live pull/push?
  //if (0 != publisher_init(main_base)) {
  //  ERR("player init failed.");
  //  return 1;
  //}

  if (0 != backend_init(main_base, &(g_conf.backend))) {
    ERR("backend init failed.");
    return 1;
  }

  TargetConfig* common_config = (TargetConfig*)ConfigManager::get_inst_config_module("common");
  string pid_dir = ".";
  if (common_config->config_file.dir.length() > 0) {
    pid_dir = common_config->config_file.dir;
  }

  ret = util_create_pid_file(pid_dir.c_str(), PROCESS_NAME);
  if (0 != ret) {
    if (ret > 0) {
      ERR("failed to create pid file. error = %s", strerror(errno));
    }
    else {
      ERR("failed to create pid file. ret = %d", ret);
    }
    server_exit();
    exit(1);
  }

  timer_service(0, 0, NULL);
  return 0;
}

int main(int argc, char **argv) {
  strcpy(g_configfile, "forward.xml");

  memset(g_ver_str, 0, sizeof(g_ver_str));
  if (strlen(SVN_REV) == strlen("r")) {
    snprintf(g_ver_str, sizeof(g_ver_str)-1, "%s", VERSION);
  }
  else {
    snprintf(g_ver_str, sizeof(g_ver_str)-1, "%s_%s", VERSION, SVN_REV);
  }

  bool to_daemon = true;
  {
    char c = 0;
    while (-1 != (c = getopt(argc, argv, "hDvc:r"))) {
      switch (c) {
      case 'D':
        to_daemon = FALSE;
        break;
      case 'c':
        strcpy(g_configfile, optarg);
        break;
      case 'v':
        printf("interlive %s\n", PROCESS_NAME);
        printf("version:  %s\n", g_ver_str);
        printf("build:    %s\n", BUILD);
        printf("branch:   %s\n", BRANCH);
        printf("hash:     %s\n", HASH);
        printf("release date: %s\n\n", DATE);
        return 0;
        break;
      case 'r':
        signal_to_reload();
        return 0;
        break;
      case 'h':
      default:
        show_help();
        return 1;
      }
    }
  }
  if (to_daemon && daemon(TRUE, 0) == -1) {
    ERR("failed to be a daemon");
    exit(1);
  }

  int ret = 0;
  ret = backtrace_init(".", PROCESS_NAME, g_ver_str);
  if (0 != ret) {
    fprintf(stderr, "backtrace init failed. ret = %d\n", ret);
    return 1;
  }
  ret = levent_init();
  if (0 != ret) {
    fprintf(stderr, "levent_init failed. ret = %d\n", ret);
    return 1;
  }

  main_base = event_base_new();
  if (NULL == main_base) {
    ERR("can't init libevent");
    return 1;
  }

  signal(SIGINT, (__sighandler_t)exit_sig_handler);
  signal(SIGTERM, (__sighandler_t)exit_sig_handler);
  signal(SIGUSR1, (__sighandler_t)reload_config_sig_handler);
  signal(SIGUSR2, (__sighandler_t)reload_config_sig_handler);

#ifdef gperf
  HeapProfilerStart("/tmp/receiver/re");
  printf("HeapProfilerStart\n");
#endif

  strcpy(g_public_ip[0], "192.168.245.133");
  g_public_cnt = 1;
  main_proc();

  event_base_dispatch(main_base);
  INF("exiting...");
  exit(0);
}
