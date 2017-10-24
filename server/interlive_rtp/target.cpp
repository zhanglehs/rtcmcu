/*******************************************
 * YueHonghui, 2013-06-05
 * hhyue@tudou.com
 * copyright:youku.com
 * ****************************************/

#include "config/ConfigManager.h"
#include "config/TargetConfig.h"
#include "connection_manager/FlvConnectionManager.h"
#include "connection_manager/RtpConnectionManager.h"
#include "relay/RelayManager.h"
#include "relay/flv_publisher.h"
#include "rtp_trans/rtp_trans_manager.h"
#include "define.h"
#include "perf.h"
#include "info_collector.h"
#include "network/base_http_server.h"
#include "../util/access.h"
#include "../util/backtrace.h"
#include "../util/log.h"
#include "../util/type_defs.h"
#include "../util/util.h"
//#define gperf
#ifdef gperf
#include <google/heap-profiler.h>
#endif

char g_public_ip[1][32];
int  g_public_cnt;
char g_private_ip[1][32];
int  g_private_cnt;
char g_ver_str[64];
char g_process_name[PATH_MAX] = { 0 };

static char g_configfile[PATH_MAX];
static struct event_base *main_base = NULL;
static struct event g_ev_timer;
static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_reload_config = 0;
static Perf *perf = NULL;

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

static void show_help(void) {
  fprintf(stderr, "%s %s, Build-Date: " __DATE__ " " __TIME__ "\n"
    "Usage:\n" " -h this message\n"
    " -c file, config file, default is %s.xml\n"
    " -r reload config file\n" " -D don't go to background\n"
    " -v verbose\n\n", g_process_name, g_ver_str, g_process_name);
}

static void server_exit()
{
  INF("player fini...");
  LiveConnectionManager::DestroyInstance();
  INF("uploader fini...");
  RtpUdpServerManager::DestroyInstance();
  RtpTcpServerManager::DestroyInstance();
  INF("cache manager fini...");
  media_manager::FlvCacheManager::DestroyInstance();
  INF("access fini...");
  access_fini();
  INF("Perf fini...");
  Perf::destory();
  INF("begin to backtrace fini...");
  backtrace_fini();
  INF("program stopping...");
  log_fini();
  //event_base_free(main_base);
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

    int ret = config_manager.ParseConfigFile();
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
  snprintf(filename, sizeof(filename)-1, "%s.pid", g_process_name);

  std::string pidfile = common_config->config_file.dir + std::string(filename);

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

static int InitLog(TargetConfig *config) {
  char* host_ip = NULL;
  host_ip = g_public_cnt > 0 ? (char*)g_public_ip[0] : (g_private_cnt > 0 ? (char*)g_private_ip[0] : NULL);
  if (NULL == host_ip) {
    fprintf(stderr, "no public or private ip found.\n");
    return -1;
  }

  if (LOGGER_FILE == config->logger_mode) {
    char mkcmd[PATH_MAX] = "logs/";
    sprintf(mkcmd, "mkdir -p %s", config->logdir);
    system(mkcmd);
    chmod(config->logdir, 0777);

    int ret = log_init_file(config->logdir, g_process_name, LOG_LEVEL_DBG, 300 * 1024 * 1024);
    if (0 != ret) {
      fprintf(stderr, "logdir = %s, log init file failed. ret = %d\n",
        config->logdir, ret);
      return -1;
    }
    char accesspath[PATH_MAX];

    accesspath[PATH_MAX - 1] = 0;
    snprintf(accesspath, PATH_MAX - 1, "%s_access", g_process_name);
    ret = access_init_file(config->logdir, accesspath, 300 * 1024 * 1024);
    if (0 != ret)
    {
      fprintf(stderr, "access init failed. logdir = %s, ret = %d\n",
        config->logdir, ret);
      return -1;
    }
  }

  log_set_level(config->log_level);
  log_set_max_size(config->log_cut_size_MB * 1024 * 1024);
  access_set_max_size(config->access_cut_size_MB * 1024 * 1024);

  return 0;
}

static int main_proc() {
  if (0 != resolv_ip_addr()) {
    ERR("resolv ip address failed.");
    return -1;
  }

  if (ConfigManager::get_inst().Init(g_configfile) < 0) {
    ERR("parse xml %s failed", g_configfile);
    return 1;
  }

  TargetConfig *common_config = (TargetConfig *)ConfigManager::get_inst_config_module("common");
  if (InitLog(common_config) < 0) {
    return 1;
  }

  INF("on boot.after resolv...");
  ConfigManager::get_inst().dump_config();

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

  media_manager::FlvCacheManager::Instance()->Init(main_base);

  /* ignore SIGPIPE when write to closing fd
   * otherwise EPIPE error will cause transporter to terminate unexpectedly
   */
  signal(SIGPIPE, SIG_IGN);

  HttpServerManager::Instance()->Init(main_base);
  //FlvCacheManager::Instance()->set_http_server(base_http_server);

  perf = Perf::get_instance();
  perf->set_cpu_rate_threshold(common_config->cpu_rate_threshold);

  RTPTransManager::Instance()->Init(main_base);
  RtpTcpServerManager::Instance()->Init(main_base);
  RtpUdpServerManager::Instance()->Init(main_base);
  RelayManager::Instance()->Init(main_base);

  if (0 != LiveConnectionManager::Instance()->Init(main_base)) {
    ERR("player init failed.");
    return 1;
  }

  std::string pid_dir = ".";
  if (common_config->config_file.dir.length() > 0) {
    pid_dir = common_config->config_file.dir;
  }
  int ret = util_create_pid_file(pid_dir.c_str(), g_process_name);
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

static int GetProcessName() {
  char binary_full_path[PATH_MAX];
  memset(binary_full_path, 0, sizeof(binary_full_path));
  int cnt = readlink("/proc/self/exe", binary_full_path, PATH_MAX);
  if (cnt < 0 || cnt >= PATH_MAX) {
    fprintf(stderr, "resolve binary file path failed. \n");
    return -1;
  }
  char *filename = strrchr(binary_full_path, '/');
  if (filename == NULL) {
    fprintf(stderr, "resolve binary file path failed. \n");
    return -1;
  }
  strcpy(g_process_name, filename + 1);
  return 0;
}

int main(int argc, char **argv) {
  if (GetProcessName() < 0) {
    return -1;
  }

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
        printf("process:  %s\n", g_process_name);
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
  ret = backtrace_init(".", g_process_name, g_ver_str);
  if (0 != ret) {
    fprintf(stderr, "backtrace init failed. ret = %d\n", ret);
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
