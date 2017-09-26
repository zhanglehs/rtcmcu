#include "info_collector.h"

#include "common/type_defs.h"
#include "util/log.h"
#include "module_tracker.h"
#include "target.h"
#include <common/protobuf/InterliveServerStat.pb.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <stdio.h>
#include <string>
using std::string;

extern "C"
{
#include <stat/st_common.h>
#include <stat/st_rdstats.h>
}

#include <stat/stat_proc_pid.h>
#include <stat/status_proc_pid.h>
#ifdef NEW_FORWARD_SERVER
#include "backend_new/forward_server.h"
using namespace interlive::forward_server;
#else
#include "backend/forward_server_v2.h"
#include "backend/forward_client_v2.h"
#endif

#include "player/module_player.h"

#define IC_MAX_PROC_NAME 20
#define IC_SHELL_BUFFER_MAX 512
#define IC_CMD_GET_PROCESSOR_COUNT "cat /proc/cpuinfo | grep processor | wc -l"
#define IC_CMD_GET_PROCESS_NAME "/proc/%d/exe"
#define IC_IFACE_MAX 5
#define IC_PB_BUFFER_SIZE 1024

struct PBBuffer
{
  char buff[IC_PB_BUFFER_SIZE];
  size_t size;
};

struct InfoStoreTemp
{
  // !! time there is original value, thus they are multipled by jiffies and cores !!

  unsigned long long last_uptime;
  unsigned long long last_cpu_idle;
  int pid;
  int public_iface_index;

  unsigned long long last_cpu_use;
  unsigned long long last_proc_cpu_use;

  long jiffies;

  InfoStoreTemp()
  {
    memset(this, 0, sizeof(InfoStoreTemp));
    public_iface_index = -1;
    jiffies = sysconf(_SC_CLK_TCK); // jiffies to second
  }
};

struct InfoStore
{
  // !! time there is original value, thus they are multipled by jiffies and cores !!

  time_t sys_uptime; // Seconds since system boot. collect:0|10|
  short sys_cpu_cores; // CPU cores count. collect:0|
  double sys_cpu_idle; // Percentage of current system CPU IDLE. collect:10|
  uint32_t sys_mem_total; // Total physical memory of system in KB. collect:0|
  uint32_t sys_mem_total_free; // Total availible physical memory of current system in KB. Equals to MemFree+Buffers+Cached. collect:0|10|
  uint64_t sys_net_rx; // Total received Bytes (send in MB) of the first public network. collect:10|
  uint64_t sys_net_tx; // Total sent Bytes (send in MB) of the first public network. collect:10|
  double proc_cpu_use; // Percentage of current process CPU use. collect:10|
  short proc_sleepavg; // Percentage of current process CPU sleep average. collect:10|
  uint32_t proc_vmpeak; // Peak alloced physical memory of process in KB. collect:10|
  uint32_t proc_vmsize; // Total used physical memory of process in KB. collect:10|
  uint32_t proc_vmrss; // really used physical memory of process in KB. collect:10|
  //uint32_t proc_read_bytes; // Total IO read of process in Bytes. collect:10|
  //uint32_t proc_write_bytes; // Total IO write of process in Bytes. collect:10|
  time_t proc_total_uptime; // Process running time in Seconds. collect:10|
  char proc_process_name[IC_MAX_PROC_NAME]; // Process name. collect:0|
  uint16_t buss_fsv2_stream_count; // forward server v2 current stream count. collect:10|
  uint16_t buss_fsv2_total_session; // forward server v2 session count. collect:10|
  uint16_t buss_fsv2_active_session; // forward server v2 active session count. collect:10|
  uint16_t buss_fsv2_connection_count; // forward server v2 current connection count. collect:10|
  uint16_t buss_fcv2_stream_count; // forward client v2 current stream count. collect:10|
  //uint16_t buss_uploader_connection_count; // Current uploading stream count. collect:10|//#todo delete
  //uint16_t buss_player_connection_count; // Current downloading stream count. collect:10|//#todo delete
  uint16_t buss_player_online_cnt; // player_stat_t::online_cnt. collect:10|//#todo new

  InfoStore()
  {
    memset(this, 0, sizeof(InfoStore));
  }
};

static InfoStoreTemp g_InfoStoreTemp;
static InfoStore g_InfoStore;
static InfoCollector g_InfoCollector;

static size_t read_shell(const char* cmd, char* buffer, size_t buffer_size, int* exit_code = nullptr)
{
  ASSERTR(cmd != nullptr, 0);

  FILE* fcmd = popen(cmd, "r");
  if (fcmd == nullptr)
    return 0;

  size_t red = fread(buffer, sizeof(char), buffer_size - 1, fcmd);

  int exit_status = pclose(fcmd);
  if (exit_code != nullptr)
    *exit_code = WEXITSTATUS(exit_status);

  return red;
}

class MTClientInfoCollector : public ModTrackerClientBase
{
public:
  MTClientInfoCollector() : ModTrackerClientBase()
  {
  }

  virtual ~MTClientInfoCollector()
  {
  }

  virtual int encode(const void* data, buffer* wb)
  {
    ASSERTR(data != nullptr, -1);

    const proto_wrapper* packet = reinterpret_cast<const proto_wrapper*>(data);
    ASSERTR(packet->header != nullptr && packet->payload != nullptr, -1);

    PBBuffer* pb_buff = (PBBuffer*)(packet->payload);
    proto_t cmd = packet->header->cmd;
    ASSERTR(cmd > 0, -1);

    int status = -1;
    int size = 0;

    switch (cmd)
    {
    case CMD_IC2T_SERVER_STAT_REQ:
      size = sizeof(proto_header)+pb_buff->size;
      status = encode_header_v3(wb, CMD_IC2T_SERVER_STAT_REQ, size);
      if (status != -1)
        status = buffer_append_ptr(wb, (void *)(pb_buff->buff), pb_buff->size);
      break;
    default:
      ASSERTR(false, -1);
    }

    if (status == -1)
    {
      DBG("MTClientInfoCollector::encode, cmd=%d, status=%d", int(cmd), int(status));
      return -1;
    }
    else
    {
      return size;
    }
  }

  virtual int decode(buffer* rb)
  {
    ASSERTR(rb != nullptr, -1);
    int ret = 0;

    proto_header header;
    ASSERTR(decode_header(rb, &header) != -1, -1);
    ret += sizeof(proto_header);

    rb->_start += sizeof(proto_header);

    switch (header.cmd)
    {
    case CMD_IC2T_SERVER_STAT_RSP:
    {
      f2t_server_stat_rsp msg;
      ASSERTR(decode_f2t_server_stat_rsp(&msg, rb), -1);
      ret += sizeof(f2t_server_stat_rsp);

      if (msg.result != TRACKER_RSP_RESULT_OK)
        ERR("tracker result wrong, result=%d", int(msg.result));
    }
      break;
    default:
      ASSERTR(false, -1);
    }

    rb->_start -= sizeof(proto_header);
    return ret;
  }

  virtual void on_error(short which)
  {
    g_InfoCollector._0s_sent = false;

    WRN("MTClientInfoCollector::on_error, which=%d, tc=%lu, ts=%lu",
      int(which), g_InfoCollector.total_collect, g_InfoCollector.total_send);
  }
};

static MTClientInfoCollector g_MTClientInfoCollector;

InfoCollector::InfoCollector() :
_0s_collected(false), _0s_sent(false), total_collect(0), total_send(0)
{
}

InfoCollector::~InfoCollector()
{
}

void InfoCollector::on_second(time_t t)
{
  if (!_0s_collected)
  {
    collect_info_0s();
  }

  // set _0s_sent to true if report to tracker success
  if (!_0s_sent)
  {
    send_info_0s();
  }

  if (t % 3 == 0)
  {
    collect_info_10s();
    send_info_10s();
  }
}

void InfoCollector::collect_info_0s()
{
  get_HZ();

  {
    char rsbuf[IC_SHELL_BUFFER_MAX];
    int exit_code = 0;
    size_t bufsize = read_shell(IC_CMD_GET_PROCESSOR_COUNT, rsbuf, sizeof(rsbuf), &exit_code);

    if (bufsize > 0 && exit_code == 0)
      g_InfoStore.sys_cpu_cores = atoi(rsbuf);
  }

  {
    unsigned long long _uptime;
    read_uptime(&_uptime);
    g_InfoStore.sys_uptime = _uptime;
  }

  {
    struct stats_cpu _st_cpu;
    unsigned long long _uptime;
    read_stat_cpu(&_st_cpu, 0, &_uptime, &_uptime);

    g_InfoStoreTemp.last_cpu_idle = _st_cpu.cpu_idle;
    g_InfoStoreTemp.last_uptime = _uptime;

    g_InfoStoreTemp.last_cpu_use = _st_cpu.cpu_user + _st_cpu.cpu_nice + _st_cpu.cpu_sys + _st_cpu.cpu_idle +
      _st_cpu.cpu_iowait + _st_cpu.cpu_hardirq + _st_cpu.cpu_softirq + _st_cpu.cpu_steal + _st_cpu.cpu_guest;
    g_InfoStoreTemp.last_proc_cpu_use = 0;
  }

  {
    struct stats_memory _st_memory;
    read_meminfo(&_st_memory);

    g_InfoStore.sys_mem_total = _st_memory.tlmkb;
    g_InfoStore.sys_mem_total_free = _st_memory.frmkb + _st_memory.bufkb + _st_memory.camkb;
  }

  {
    g_InfoStoreTemp.pid = getpid();
    char cmdbuf[IC_SHELL_BUFFER_MAX];
    snprintf(cmdbuf, sizeof(cmdbuf), IC_CMD_GET_PROCESS_NAME, g_InfoStoreTemp.pid);

    char rsbuf[IC_SHELL_BUFFER_MAX];
    memset(rsbuf, 0, IC_SHELL_BUFFER_MAX);
    ssize_t bufsize = readlink(cmdbuf, rsbuf, sizeof(rsbuf)-1);

    if (bufsize > 0)
    {
      char* proc_name = strrchr(rsbuf, '/');
      strncpy(g_InfoStore.proc_process_name, proc_name + 1, IC_MAX_PROC_NAME - 1);
    }

    if (strlen(g_InfoStore.proc_process_name) == 0)
      strcpy(g_InfoStore.proc_process_name, "null");
  }

  {
    string main_ipaddr;
    {
      if (g_public_cnt > 0)
        main_ipaddr.append(g_public_ip[0]);
      else if (g_private_cnt > 0)
        main_ipaddr.append(g_private_ip[0]);
    }

    if (!main_ipaddr.empty())
    {
      struct stats_net_dev p_st_net_dev[IC_IFACE_MAX];
      int iface_count = read_net_dev(p_st_net_dev, IC_IFACE_MAX);

      int inet_sock = socket(AF_INET, SOCK_DGRAM, 0);

      for (int i = 0; i < iface_count; i++)
      {
        const struct stats_net_dev& pnet = p_st_net_dev[i];

        struct ifreq ifr;
        strcpy(ifr.ifr_name, pnet.interface);
        if (ioctl(inet_sock, SIOCGIFADDR, &ifr) != 0)
        {
          ERR("collect_info_0s ioctl error");
          break;
        }

        string ipaddr(inet_ntoa(((struct sockaddr_in*)&(ifr.ifr_addr))->sin_addr));
        if (main_ipaddr == ipaddr)
        {
          g_InfoStoreTemp.public_iface_index = i;
          break;
        }
      }

      close(inet_sock);
    }

    if (g_InfoStoreTemp.public_iface_index == -1)
      WRN("collect_info_0s can't detect iface index");
  }

  _0s_collected = true;
  total_collect++;
}

void InfoCollector::collect_info_10s()
{
  {
    unsigned long long _uptime; // system uptime in seconds (multipled hz)
    read_uptime(&_uptime);
    g_InfoStore.sys_uptime = _uptime;
  }

  struct stats_cpu _st_cpu;
  {
    unsigned long long _uptime; // summized system uptime for all cpu(s) (multipled hz)
    read_stat_cpu(&_st_cpu, 0, &_uptime, &_uptime);

    g_InfoStore.sys_cpu_idle = 1.0 * (_st_cpu.cpu_idle - g_InfoStoreTemp.last_cpu_idle) / (_uptime - g_InfoStoreTemp.last_uptime);
    g_InfoStoreTemp.last_cpu_idle = _st_cpu.cpu_idle;
    g_InfoStoreTemp.last_uptime = _uptime;
  }

  {
    struct stats_memory _st_memory;
    read_meminfo(&_st_memory);

    g_InfoStore.sys_mem_total_free = _st_memory.frmkb + _st_memory.bufkb + _st_memory.camkb;
  }

  {
    if (g_InfoStoreTemp.public_iface_index != -1)
    {
      struct stats_net_dev p_st_net_dev[g_InfoStoreTemp.public_iface_index + 1];
      int iface_count = read_net_dev(p_st_net_dev, g_InfoStoreTemp.public_iface_index + 1);
      if (iface_count > g_InfoStoreTemp.public_iface_index)
      {
        const struct stats_net_dev& pnet = p_st_net_dev[g_InfoStoreTemp.public_iface_index];

        g_InfoStore.sys_net_rx = pnet.rx_bytes;
        g_InfoStore.sys_net_tx = pnet.tx_bytes;
      }
      else
      {
        WRN("collect_info_10s can't detect iface index");
      }
    }
  }

  {
    //test: for (size_t i = 0; i < 9000000000; i++);
    struct proc_pid_stat _stat;
    read_proc_pid_stat(_stat, g_InfoStoreTemp.pid);

    unsigned long long total_cpu_use = _st_cpu.cpu_user + _st_cpu.cpu_nice + _st_cpu.cpu_sys + _st_cpu.cpu_idle +
      _st_cpu.cpu_iowait + _st_cpu.cpu_hardirq + _st_cpu.cpu_softirq + _st_cpu.cpu_steal + _st_cpu.cpu_guest;

    unsigned long long process_cpu_use = _stat.utime + _stat.stime + _stat.cutime + _stat.cstime;
    int cpu_cores_cnt = read_cpucores_cnt();

    g_InfoStore.proc_cpu_use = 1.0 *(process_cpu_use - g_InfoStoreTemp.last_proc_cpu_use) \
      / (total_cpu_use - g_InfoStoreTemp.last_cpu_use)*cpu_cores_cnt;

    g_InfoStoreTemp.last_cpu_use = total_cpu_use;
    g_InfoStoreTemp.last_proc_cpu_use = process_cpu_use;

    g_InfoStore.proc_total_uptime = _stat.starttime;
  }

  {
    struct proc_pid_status _stat;
    read_proc_pid_status(_stat, g_InfoStoreTemp.pid);

    g_InfoStore.proc_sleepavg = _stat.SleepAVG;
    g_InfoStore.proc_vmpeak = _stat.VmPeak;
    g_InfoStore.proc_vmsize = _stat.VmSize;
    g_InfoStore.proc_vmrss = _stat.VmRSS;
  }

  {
    const ForwardServer* fsv2 = ForwardServer::get_server();
    g_InfoStore.buss_fsv2_stream_count = fsv2->get_stream_count();
    g_InfoStore.buss_fsv2_total_session = 0;
    g_InfoStore.buss_fsv2_active_session = 0;
    g_InfoStore.buss_fsv2_connection_count = 0;
    g_InfoStore.buss_fcv2_stream_count = 0;

    //const Player* player_inst = get_player_inst();
    //const player_stat_t * player_stat = player_inst->get_player_stat();
    //if (player_stat != nullptr)
    //{
    //  g_InfoStore.buss_player_online_cnt = player_stat->get_online_cnt();
    //  DBG("InfoCollector::collect_info_10s: buss_stream_count: %d, buss_player_online_cnt: %d", \
    //    g_InfoStore.buss_fcv2_stream_count, g_InfoStore.buss_player_online_cnt);
    //}
  }

  total_collect++;
}

// 发送基本的cpu、内存占用信息给tracker
void InfoCollector::send_info_0s()
{
  const ForwardServer* fsv2 = ForwardServer::get_server();

  if (!(fsv2->has_register_tracker()))
  {
    WRN("InfoCollector::send_info_0s, forward server not registered");
    return;
  }

  PBBuffer pb_buff;
  InterliveServerStat pb;

  unsigned long long _sys_uptime = g_InfoStore.sys_uptime / g_InfoStoreTemp.jiffies;
  pb.set_sys_uptime((uint64_t)_sys_uptime);
  pb.set_sys_cpu_cores((int32_t)g_InfoStore.sys_cpu_cores);
  pb.set_sys_mem_total((int32_t)g_InfoStore.sys_mem_total);
  pb.set_sys_mem_total_free((int32_t)g_InfoStore.sys_mem_total_free);
  pb.set_proc_process_name(g_InfoStore.proc_process_name);

  pb_buff.size = pb.ByteSize();
  pb.SerializeToArray(pb_buff.buff, pb_buff.size);
  proto_header ph = { 0, 0, CMD_IC2T_SERVER_STAT_REQ, 0 };
  proto_wrapper pw = { &ph, (void *)&pb_buff };
  bool ret = ModTracker::get_inst().send_request(&pw, &g_MTClientInfoCollector);
  if (ret)
  {
    _0s_sent = true;
    total_send++;
  }
  else
  {
    WRN("InfoCollector::send_info_0s end error, send_data size=%lu", pb_buff.size);
  }
}

// 发送cpu、内存、网络、流数目、连接数等基本信息给tracker
void InfoCollector::send_info_10s()
{
  if (!_0s_sent)
    return;

  const ForwardServer* fsv2 = ForwardServer::get_server();

  if (!(fsv2->has_register_tracker()))
  {
    WRN("InfoCollector::send_info_10s, forward server not registered");
    return;
  }


  PBBuffer pb_buff;
  InterliveServerStat pb;

  unsigned long long _sys_uptime = g_InfoStore.sys_uptime / g_InfoStoreTemp.jiffies;
  pb.set_sys_uptime((uint64_t)_sys_uptime);
  int _sys_cpu_idle_100 = int32_t(100.0 * g_InfoStore.sys_cpu_idle);
  pb.set_sys_cpu_idle(_sys_cpu_idle_100);
  pb.set_sys_mem_total_free((int32_t)g_InfoStore.sys_mem_total_free);
  pb.set_sys_net_rx(g_InfoStore.sys_net_rx);
  pb.set_sys_net_tx(g_InfoStore.sys_net_tx);
  int _proc_cpu_use = int32_t(100.0 * g_InfoStore.proc_cpu_use + 0.5);
  pb.set_proc_cpu_use(_proc_cpu_use);
  pb.set_proc_sleepavg((int32_t)g_InfoStore.proc_sleepavg);
  pb.set_proc_vmpeak((int32_t)g_InfoStore.proc_vmpeak);
  pb.set_proc_vmsize((int32_t)g_InfoStore.proc_vmsize);
  pb.set_proc_vmrss((int32_t)g_InfoStore.proc_vmrss);
  unsigned long long _proc_total_uptime = _sys_uptime - g_InfoStore.proc_total_uptime / HZ;
  pb.set_proc_total_uptime((uint64_t)_proc_total_uptime);
  pb.set_buss_fsv2_stream_count((int32_t)g_InfoStore.buss_fsv2_stream_count);
  pb.set_buss_fsv2_total_session((int32_t)g_InfoStore.buss_fsv2_total_session);
  pb.set_buss_fsv2_active_session((int32_t)g_InfoStore.buss_fsv2_active_session);
  pb.set_buss_fsv2_connection_count((int32_t)g_InfoStore.buss_fsv2_connection_count);
  pb.set_buss_fcv2_stream_count((int32_t)g_InfoStore.buss_fcv2_stream_count);
  //pb.set_buss_uploader_connection_count((int32_t)g_InfoStore.buss_uploader_connection_count);//#todo
  pb.set_buss_player_online_cnt((int32_t)g_InfoStore.buss_player_online_cnt);//#todo

  pb_buff.size = pb.ByteSize();
  pb.SerializeToArray(pb_buff.buff, pb_buff.size);

  proto_header ph = { 0, 0, CMD_IC2T_SERVER_STAT_REQ, 0 };
  proto_wrapper pw = { &ph, (void *)&pb_buff };
  bool ret = ModTracker::get_inst().send_request(&pw, &g_MTClientInfoCollector);
  if (ret)
  {
    _0s_sent = true;
    total_send++;
  }
  else
  {
    WRN("InfoCollector::send_info_10s end error, send_data size=%lu", pb_buff.size);
  }
}

InfoCollector& InfoCollector::get_inst()
{
  return g_InfoCollector;
}
