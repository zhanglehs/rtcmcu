/*
 * author: hechao@youku.com
 * create: 2013.5.21
 *
 */
#include "tracker.h"

#include <algorithm>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <list>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include <common/proto.h>
#include <common/protobuf/InterliveServerStat.pb.h>
#include <utils/buffer.hpp>
#include <util/levent.h>
#include <util/list.h>
#include <util/log.h>
#include <utils/memory.h>
#include <util/util.h>
#include "stream_manager.h"

#include "forward_manager_v3.h"
#include "tracker_stat.h"
#include "http_server.h"
#include "portal.h"

#include "common/protobuf/f2t_register_req.pb.h"

using namespace lcdn;
using namespace lcdn::net;
using namespace lcdn::base;

extern tracker_stat_t            *g_tracker_stat;
extern tracker_config_t          *g_config;

static Portal                    *g_portal;
static StreamManager             *g_stream_manager;
static StreamListNotifier        *g_stream_list_notifier;

static struct event_base         *ev_base = NULL;
static struct event              listen_ev;
static struct event              timer_ev;
static int                       listen_fd = -1;
static bool                      g_stream_list_buffer_stale = true;
static int last_report_time = 0;
const int  REPORT_INTERVAL  = 120;
const static int PROTO_HEADER_LEN = 8;
const static int MT2T_HEADER_LEN = 4;

using namespace std;

typedef struct client
{
    struct event ev;
    int fd;
    uint32_t ip;        /* got from register request*/
    uint32_t fdip;        /* got from fd*/
    //uint16_t port;
    uint64_t fid;
    //uint32_t streamid;
    buffer *rb, *wb;

}client_t;

typedef std::list<client*> client_list_t;
client_list_t g_clients;

static void tracker_accept_handler(int fd, short which, void *arg);
static void tracker_server_handler(int fd, short which, void* arg);
static void tracker_timer_handler(int fd, short which, void *arg);
static void tracker_read_handler(int fd, client_t *c);
static void tracker_write_handler(int fd, client_t *c);
static void enable_forward_write(client_t *c);
static void disable_forward_write(client_t *c);

static int tracker_process_addr_req_v3(client_t *c);
static int tracker_process_update_stream_map_v3(client_t *c);
static int tracker_process_keep_alive_v3(client_t *c);
static int tracker_process_register_v3(client_t *c);
static int tracker_process_register_v4(client_t *c);
static int tracker_process_server_stat(client_t *c);
static int tracker_process_r2p_keepalive_mt2t(client_t* c);
static int tracker_process_f2p_white_list_req_mt2t(client_t* c);
static int tracker_process_f2p_keepalive_mt2t(client_t* c);
static int tracker_process_st2t_fpinfo_req_mt2t(client_t* c);

static client_t * tracker_create_client();
static int init_client_struct(client_t *c);
static int tracker_close_client(client_t *c);
static void tracker_handle_income_data(client_t *c);

buffer* tracker_get_stream_list_buffer();
buffer* tracker_prepare_stream_list_buffer(size_t bytes);
void tracker_update_stream_list_buffer();
void tracker_broadcast_stream_close(const uint32_t stream_id);
void tracker_broadcast_stream_start(const StreamItem& item);

//static ForwardServerManager *forward_manager = NULL;
ForwardServerManager *forward_manager = NULL;

//message structure between tracker and module_tracker(protocol v2,v3).

/*
        ---------------------------
        |  proto_header(outer)    |
        |                         |
        ---------------------------
        |  mt2t_proto_header      |
        |                         |
        ---------------------------
        |  proto_header(inner)    |
        |                         |
        ---------------------------
        |  payload                |
        |                         |
        ---------------------------
*/

int
tracker_init(tracker_config_t *config)
{
    forward_manager = new ForwardServerManager;
    if (!forward_manager
            || 0 != forward_manager->init())
    {
        return -1;
    }

    if (0 != tracker_stat_init(config->log_svr_ip, config->log_svr_port))
    {
        ERR("tracker stat init failed.");
        return -1;
    }

    last_report_time = time(0);
    INF("%s","tracker_init succ");
    return 0;
}

void stream_start_cb(StreamItem& item)
{
    g_stream_list_notifier->notify_stream_start(item);
}

void stream_close_cb(uint32_t streamid)
{
    g_stream_list_notifier->notify_stream_close(streamid);
}

int
tracker_start_tracker(lcdn::net::EventLoop* loop, struct event_base *base)
{
    g_stream_manager = new StreamManager();
    g_portal = new Portal(base, true, g_stream_manager, forward_manager);
    if (0 != g_portal->init())
    {
        ERR("portal init failed.");
        return -1;
    }
    g_portal->register_create_stream(stream_start_cb);
    g_portal->register_destroy_stream(stream_close_cb);

    InetAddress listen_addr(g_config->notifier_listen_port);

    ev_base = base;
    struct sockaddr_in svr_addr;
    memset(&svr_addr, 0, sizeof(svr_addr));
    svr_addr.sin_family = AF_INET;

    /*
    if (g_config->listen_ip[0] == '\0')
        svr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        svr_addr.sin_addr.s_addr = inet_addr(g_config->listen_ip);
    */
    svr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    svr_addr.sin_port = htons(g_config->listen_port);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        ERR("create server socket error");
        return -1;
    }
    util_set_nonblock(listen_fd, TRUE);
    int val = 1;
    if(-1 == setsockopt (listen_fd, SOL_SOCKET, SO_REUSEADDR,
                (void*)&val, sizeof (val)))
    {
        ERR("set socket SO_REUSEADDR failed. error = %s", strerror(errno));
        return -5;
    }
    val = 1;
    if(-1 == setsockopt(listen_fd, IPPROTO_TCP, TCP_NODELAY,
                (void*)&val, sizeof(val)))
    {
        ERR("set tcp nodelay failed. error = %s", strerror(errno));
        return -1;
    }

    if (0 != bind(listen_fd, (const struct sockaddr *)&svr_addr, sizeof(svr_addr))
            || 0 != listen(listen_fd, 512))
    {
        ERR("bind or listen error");
        return -1;
    }

    levent_set(&listen_ev, listen_fd, EV_READ | EV_PERSIST, tracker_accept_handler, NULL);
    event_base_set(ev_base, &listen_ev);
    levent_add(&listen_ev, 0);

    /*
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    levtimer_set(&timer_ev, tracker_timer_handler, NULL);
    event_base_set(ev_base, &timer_ev);
    levtimer_add(&timer_ev, &tv);
    */

    tracker_timer_handler(0,0,0);

    INF("%s","tracker_start_tracker succ");
    return 0;
}

void
tracker_stop_tracker()
{
    //forward_manager->fm_clean_all();
    if (forward_manager)
    {
        delete forward_manager;
        forward_manager = NULL;
    }

    levent_del(&listen_ev);
    if (listen_fd < 0)
        close(listen_fd);
    INF("%s","tracker_stop_tracker");
}

void
tracker_dump_state()
{
}

void
tracker_expire_stream_list()
{
    g_stream_list_buffer_stale = true;
}

static void
tracker_timer_handler(int fd, short which, void *arg)
{
    time_t now = time(0);
    /* stat report */
    if (now < last_report_time)
        last_report_time = now;
    if (now - last_report_time >= REPORT_INTERVAL)
    {
        g_tracker_stat->cur_forward_num = forward_manager->fm_cur_forward_num();
        g_tracker_stat->cur_stream_num = forward_manager->fm_cur_stream_num();
        tracker_stat_dump_and_reset(g_tracker_stat);
        last_report_time = now;
    }

    g_portal->check_connection(g_config);

    if (now % 3 == 0)
    {
        g_portal->keepalive();
    }

    forward_manager->fm_clear_stream_cnt_per_sec();

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    levtimer_set(&timer_ev, tracker_timer_handler, NULL);
    event_base_set(ev_base, &timer_ev);
    levtimer_add(&timer_ev, &tv);
}

static void
tracker_accept_handler(int fd, short which, void *arg)
{
    int newfd;
    struct sockaddr_in s_in;
    socklen_t len = sizeof(struct sockaddr_in);

    memset(&s_in, 0, len);
    newfd = accept(fd, (struct sockaddr *) &s_in, &len);
    if(newfd < 0)
    {
        ERR("accept client failed. fd = %d. errno: %d", fd, errno);
        return;
    }
    util_set_nonblock(newfd, TRUE);

    struct sockaddr_in client_addr;
    uint32_t addr_len = sizeof(client_addr);
    if (0 != getpeername(newfd, (struct sockaddr*)&client_addr, &addr_len))
    {
        ERR("getpeername error: %s", strerror(errno));
        //tracker_close_client(c);
        close(newfd);
        newfd = -1;
        return;
    }

    client_t *c = NULL;
    c = tracker_create_client();
    if(c == NULL || !g_stream_manager->is_stream_manager_ready())
    {
        ERR("create client failed. fd = %d", newfd);
        close(newfd);
        newfd = -1;
        return;
    }
    c->fdip = ntohl(client_addr.sin_addr.s_addr);
    c->fd = newfd;

    c->fid = -1;
    TRC("accept a handler. ip: %s, fd: %u", util_ip2str(c->fdip), c->fd);

    levent_set(&c->ev, c->fd, EV_READ | EV_PERSIST,
            tracker_server_handler, (void *)c);
    event_base_set(ev_base, &c->ev);
    levent_add(&c->ev, 0);

}

static void
tracker_server_handler(int fd, short which, void* arg)
{
    DBG("tracker_server_handler: event on socket %d:%s%s%s%s",
            (int) fd,
            (which&EV_TIMEOUT) ? " timeout" : "",
            (which&EV_READ)    ? " read" : "",
            (which&EV_WRITE)   ? " write" : "",
            (which&EV_SIGNAL)  ? " signal" : "");
    client_t *c = (client_t *)arg;
    if (which & EV_READ)
        tracker_read_handler(fd, c);
    else if (which & EV_WRITE)
        tracker_write_handler(fd, c);
}

static void
tracker_read_handler(int fd, client_t *c)
{
    TRC("tracker_read_handler: fd = %d", fd);
    if (c && c->rb && c->wb)
    {
        DBG("tracker_read_handler enter: fd = %d, c->fd = %d, len(c->wb) = %lu, len(c->rb) = %lu",
            fd, c->fd, buffer_data_len(c->wb), buffer_data_len(c->rb));
    }
    int r = buffer_read_fd(c->rb, fd);
    //if (0 > buffer_read_fd(c->rb, fd))
    if (0 > r)
    {
        DBG("tracker_read_handler. read error: %u(%s), close client,fd: %d, ip: %s, fdip: %s.",
                errno, strerror(errno),fd,
                util_ip2str(c->fdip),
                util_ip2str(c->ip));
        tracker_close_client(c);
        c = NULL;
        return;
    }

    TRC("tracker_read_handler. read size: %u", r);

    tracker_handle_income_data(c);
}

static void
tracker_handle_income_data(client_t *c)
{
    while (buffer_data_len(c->rb) >= sizeof(proto_header))
    {
        DBG("tracker_handle_income_data: len(c->rb) = %u, c->fd = %d", buffer_data_len(c->rb), c->fd);

        proto_header pro_head;
        if (decode_header(c->rb, &pro_head) != 0)
        {
            INF("tracker_handle_income_data error: decode_header failed fd = %d", c->fd);
            tracker_close_client(c);
            return;
        }

        if (pro_head.size > 1024 * 1024) {
            WRN("tracker_handle_income_data: header size = %u > 1MB", pro_head.size);
            tracker_close_client(c);
            return;
        }

        DBG("tracker_handle_income_data: len(c->rb) = %u, head.size = %u, c->fd = %d", buffer_data_len(c->rb), pro_head.size, c->fd);
        if (buffer_data_len(c->rb) < pro_head.size)
        {
            // not enough data received
            return;
        }

        int result = -1;
        proto_header* ih = (proto_header*)(buffer_data_ptr(c->rb)
            + sizeof(proto_header) + sizeof(mt2t_proto_header));

        switch(ntohs(ih->cmd))
        {
            case CMD_FC2T_ADDR_REQ_V3:
                DBG("tracker_handle_income_data: CMD_FC2T_ADDR_REQ_V3 fid = %lu", c->fid);
                g_tracker_stat->req_addr_cnt++;
                result = tracker_process_addr_req_v3(c);
                break;
            case CMD_FS2T_UPDATE_STREAM_REQ_V3:
                DBG("tracker_handle_income_data: CMD_FS2T_UPDATE_STREAM_REQ_V3 fid = %lu", c->fid);
                result = tracker_process_update_stream_map_v3(c);
                break;
            case CMD_FS2T_KEEP_ALIVE_REQ_V3:
                DBG("tracker_handle_income_data: CMD_FS2T_KEEP_ALIVE_REQ_V3 fid = %lu", c->fid);
                g_tracker_stat->keep_req_cnt++;
                result = tracker_process_keep_alive_v3(c);
                break;
            case CMD_FS2T_REGISTER_REQ_V3:
                DBG("tracker_handle_income_data: CMD_FS2T_REGISTER_REQ_V3 fid = %lu", c->fid);
                g_tracker_stat->register_cnt++;
                result = tracker_process_register_v3(c);
                break;
            case CMD_FS2T_REGISTER_REQ_V4:
                DBG("tracker_handle_income_data: CMD_FS2T_REGISTER_REQ_V4 fid = %lu", c->fid);
                g_tracker_stat->register_cnt++;
                result = tracker_process_register_v4(c);
                break;
            case CMD_IC2T_SERVER_STAT_REQ:
                DBG("tracker_handle_income_data: CMD_IC2T_SERVER_STAT_REQ fid = %lu", c->fid);
                result = tracker_process_server_stat(c);
                break;
// folllowing commands are ported from stream_list_notifier
            case CMD_R2P_KEEPALIVE:
                DBG("tracker_handle_income_data: CMD_R2P_KEEPALIVE fid = %lu", c->fid);
                result = tracker_process_r2p_keepalive_mt2t(c);
                break;
            case CMD_F2P_WHITE_LIST_REQ:
                DBG("tracker_handle_income_data: CMD_F2P_WHITE_LIST_REQ fid = %lu", c->fid);
                result = tracker_process_f2p_white_list_req_mt2t(c);
                break;
            case CMD_F2P_KEEPALIVE:
                DBG("tracker_handle_income_data: CMD_F2P_KEEPALIVE fid = %lu", c->fid);
                // todo: dig further on f2p_keepalive
                result = tracker_process_f2p_keepalive_mt2t(c);
                break;
            case CMD_ST2T_FPINFO_REQ:
                DBG("tracker_handle_income_data: CMD_F2P_WHITE_LIST_REQ fid = %lu", c->fid);
                result = tracker_process_st2t_fpinfo_req_mt2t(c);
                break;
            default:
                ERR("tracker_handle_income_data error: unrecognized command = %u (proto_header.version = 3)", ntohs(ih->cmd));
        }

        if(-2 == result)
        {
            WRN("process server stat error,but NOT close client. fid: %lu, cmd: %u", c->fid, pro_head.cmd);
            return;
        }
        else if (-1 == result)
        {
            WRN("handle request error. fid: %lu, cmd: %u", c->fid, pro_head.cmd);
            tracker_close_client(c);
            return;
        }
    } // while(buffer_data_len(c->rb) >= sizeof(proto_header))
    buffer_try_adjust(c->rb);
}

static void
tracker_write_handler(int fd, client_t *c)
{
    if (c && c->rb && c->wb)
    {
        DBG("tracker_write_handler enter: fd = %d, c->fd = %d, len(c->wb) = %lu, len(c->rb) = %lu",
            fd, c->fd, buffer_data_len(c->wb), buffer_data_len(c->rb));
    }
    if (buffer_data_len(c->wb) > 0)
    {
        if (buffer_write_fd(c->wb, c->fd) < 0)
        {
            ERR("tracker_write_handler error: %u(%s), fd: %d",
                errno, strerror(errno),fd);
            tracker_close_client(c);
            return;
        }
        buffer_try_adjust(c->wb);
    }
    else
    {
        disable_forward_write(c);
    }
}

static void
enable_forward_write(client_t *c)
{
    if(c)
    {
        levent_del(&c->ev);
        levent_set(&c->ev, c->fd, EV_READ | EV_WRITE | EV_PERSIST,
                tracker_server_handler, (void *)c);
        event_base_set(ev_base, &c->ev);
        levent_add(&c->ev, 0);
    }
}

static void
disable_forward_write(client_t *c)
{
    if(c)
    {
        levent_del(&c->ev);
        levent_set(&c->ev, c->fd, EV_READ | EV_PERSIST,
                tracker_server_handler, (void *)c);
        event_base_set(ev_base, &c->ev);
        levent_add(&c->ev, 0);
    }
}

static int
init_client_struct(client_t *c)
{
    c->fd = -1;
    c->ip = 0;
    c->rb = buffer_init(10*1024);
    if (NULL == c->rb)
        return -1;
    c->wb = buffer_init(10*1024);
    if (NULL == c->wb)
    {
        buffer_free(c->wb);
        return -1;
    }

    return 0;
}

static client_t *
tracker_create_client()
{
    client_t *c = (client_t *) mcalloc(1, sizeof(*c));
    if(c == NULL)
    {
        ERR("client struct calloc failed. out of memory!");
    }
    else if(init_client_struct(c) != 0)
    {
        mfree(c);
        c = NULL;
        ERR("init forward struct failed.");
    }

    levent_set(&c->ev, -1, EV_READ | EV_PERSIST,
            NULL, NULL);
    levent_del(&c->ev);
    return c;
}

static int
tracker_close_client(client_t *c)
{
    DBG("tracker_close_client. fid: %lu, ip: %s, fdip: %s, fd: %u",
            c->fid,
            util_ip2str(c->ip),
            util_ip2str(c->fdip), c->fd);

    if (c->fid != uint64_t(-1))
    {
        forward_manager->fm_del_a_forward(c->fid);
    }
    g_clients.erase(std::remove(g_clients.begin(), g_clients.end(), c), g_clients.end());
    assert(c->fd > 0);
    if (c->fd > 0)
    {
        close(c->fd);
        c->fd = -1;
        levent_del(&(c->ev));
    }

    buffer_free(c->rb);
    c->rb = NULL;
    buffer_free(c->wb);
    c->wb = NULL;

    mfree(c);
    c = NULL;

    return 0;
}

static int
tracker_process_register_v3(client_t *c)
{
    // decode outer proto header and tracker header
    proto_header pro_header_outer;
    mt2t_proto_header tracker_header;

    if (decode_f2t_header_v3(c->rb, &pro_header_outer, &tracker_header) != 0)
    {
        ERR("decode outer protocol header error when parsing registering req v3.");
        return -1;
    }

    if (buffer_data_len(c->rb) < pro_header_outer.size)
    {
        ERR("invalid package found when parsing registering req v3.");
        return -1;
    }

    if (CMD_MT2T_REQ_RSP != pro_header_outer.cmd)
        return -1;

    buffer_eat(c->rb, sizeof(proto_header) + sizeof(mt2t_proto_header));
    DBG("tracker process mt2t, seq=%d", int(tracker_header.seq));

    // decode inner proto header
    proto_header pro_header_inner;
    if (decode_header(c->rb, &pro_header_inner) != 0)
    {
        ERR("decode inner protocol header error when parsing registering req v3.");
        return -1;
    }

    if (buffer_data_len(c->rb) < pro_header_inner.size)
    {
        ERR("length of request packet is not enough when parsing registering req v3.");
        return -1;
    }

    if (CMD_FS2T_REGISTER_REQ_V3 != pro_header_inner.cmd)
        return -1;

    buffer_eat(c->rb, sizeof(proto_header));

    // decode payload
    f2t_register_req_v3 req;
    f2t_register_rsp_v3 rsp;
    if (! decode_f2t_register_req_v3(&req, c->rb))
    {
        rsp.result = TRACKER_RSP_RESULT_BADREQ;
    }
    else
    {
        INF("a client registering v3: %s", util_ip2str(req.ip));
        uint16_t asn=-1, region=-1;
        uint8_t level=-1;
        uint32_t group_id = -1;

        if (0 != tc_get_asn_and_region_and_level(req.ip, &asn, &region, &level))
        {
            WRN("found unrecognizing forward v3. ip: %s", util_ip2str(req.ip));
            return -1;
        }

        if (0 != tc_get_group_id(req.ip, &group_id))
        {
            WRN("found unrecognizing forward v3. ip: %s", util_ip2str(req.ip));
            return -1;
        }

        c->ip = req.ip;
        req.asn = asn;
        req.region = region;
        c->fid = forward_manager->fm_add_a_forward_v3(&req, level, group_id);
        if (c->fid == uint64_t(-1))
        {
            ERR("register client error v3. ip: %s; port: %u",
                util_ip2str(req.ip), req.port);
            rsp.result = TRACKER_RSP_RESULT_INNER_ERR;
        }
        else
        {
            INF("a client registered v3. fid: %lu; ip: %s; port: %u; topo_level: %u",
                    c->fid, util_ip2str(req.ip), req.port, level);
            rsp.result = TRACKER_RSP_RESULT_OK;
            g_clients.push_back(c);

            /* add aliases if there exist */
            const char * alias = tc_get_alias(req.ip);
            if (alias)
            {
                const char * s = alias, *e = alias;
                while ((e = strchr(s, ';')) != NULL)
                {
                    char str_ip[32] = {0};
                    uint32_t ip;
                    uint16_t asn=-1, region=-1;
                    if (3 != sscanf(s, "%[^:]:%hu:%hu", str_ip, &asn, &region))
                    {
                        WRN("parse alias error.");
                        break;
                    }
                    INF("add alias v3: ip: %s, asn: %u, region: %u", str_ip, asn, region);
                    if (0 != util_str2ip(str_ip, &ip))
                    {
                        ERR("convert string ip to int ip error.");
                        break;
                    }
                    forward_manager->fm_add_alias(c->fid, ip, asn, region);
                    s = e+1;
                    if (*s == '\0')
                        break;
                }
            }
        }
    }

    buffer_eat(c->rb, sizeof(f2t_register_req_v3));

    // encode outer proto header
    uint32_t total_sz_outer = sizeof(proto_header) + sizeof(mt2t_proto_header) +
                        sizeof(proto_header) + sizeof(f2t_register_rsp_v3);

    if (encode_header_v3(c->wb, CMD_MT2T_REQ_RSP, total_sz_outer) != 0)
    {
        ERR("tracker_process_register_v3 encode outer protocol header error.");
        return -1;
    }

    // encode tracker header
    if (encode_mt2t_header(c->wb, tracker_header) != 0)
    {
        ERR("tracker_process_register_v3 encode tracker header error.");
        return -1;
    }

    // encode inner proto header
    uint32_t total_sz_inner = sizeof(proto_header) + sizeof(f2t_register_rsp_v3);

    if (encode_header_v3(c->wb, CMD_FS2T_REGISTER_RSP_V3, total_sz_inner) != 0)
    {
        ERR("tracker_process_register_v3 encode inner protocol header error.");
        return -1;
    }

    // encode payload
    if (encode_f2t_register_rsp_v3(&rsp, c->wb) != 0)
    {
        ERR("tracker_process_register_v3 encode_f2t_register_rsp_v3 error.");
        return -1;
    }

    enable_forward_write(c);
    DBG("tracker encode rsp mt2t, seq=%d", int(tracker_header.seq));
    return 0;
}

static int
tracker_process_addr_req_v3(client_t *c)
{
    // decode outer proto header and tracker header
    proto_header pro_header_outer;
    mt2t_proto_header tracker_header;

    if (decode_f2t_header_v3(c->rb, &pro_header_outer, &tracker_header) != 0)
    {
        ERR("decode outer protocol header error when parsing addr req v3.");
        return -1;
    }

    if (buffer_data_len(c->rb) < pro_header_outer.size)
    {
        ERR("invalid package found when parsing addr req v3.");
        return -1;
    }

    if (CMD_MT2T_REQ_RSP != pro_header_outer.cmd)
        return -1;

    buffer_eat(c->rb, sizeof(proto_header) + sizeof(mt2t_proto_header));
    DBG("tracker process mt2t, seq=%d", int(tracker_header.seq));

    // decode inner proto header
    proto_header pro_header_inner;
    if (decode_header(c->rb, &pro_header_inner) != 0)
    {
        ERR("decode inner protocol header error when parsing addr req v3.");
        return -1;
    }

    if (buffer_data_len(c->rb) < pro_header_inner.size)
    {
        ERR("length of request packet is not enough when parsing addr req v3.");
        return -1;
    }

    if (CMD_FC2T_ADDR_REQ_V3 != pro_header_inner.cmd)
        return -1;

    buffer_eat(c->rb, sizeof(proto_header));

    //decode payload
    f2t_addr_req_v3 req;
    f2t_addr_rsp_v3 rsp;
    if (! decode_f2t_addr_req_v3(&req, c->rb))
    {
        rsp.result = TRACKER_RSP_RESULT_BADREQ;
    }
    else
    {
        string log_streamid;
        forward_manager->convert_streamid_array_to_string(req.streamid, log_streamid);

        INF("a client requesting addr v3: %s", util_ip2str(req.ip));
        uint16_t asn, region;
        uint8_t level = -1;
        if (0 != tc_get_asn_and_region_and_level(req.ip, &asn, &region, &level))
        {
            WRN("found unrecognizing forward request v3. ip: %s", util_ip2str(c->ip));
            return -1;
        }
        {
            req.asn = asn;
            req.region = region;
            req.level = level;
        }

        int ret = -1;
        if ((req.asn == ASN_TEL)||(req.asn == ASN_CNC)) // for original system
        {
            ret = forward_manager->fm_get_proper_forward_v3(&req, &rsp);
        }
        else if ((req.asn == ASN_TEL_RTP) || (req.asn == ASN_CNC_RTP)) // for rtp system
        {
            ret = forward_manager->fm_get_proper_forward_v3(&req, &rsp);
        }
        if (ret != 0)
        {
            WRN("get proper forward error v3. streamid: %s , req_ip: %s",
                log_streamid.c_str(), util_ip2str(req.ip));
            rsp.result = TRACKER_RSP_RESULT_NORESULT;
        }
        else
        {
            INF("a client request addr done. get proper forward succ v3. streamid: %s, rsp_ip: %s, rsp_port: %u, level: %u",
                log_streamid.c_str(), util_ip2str(rsp.ip),rsp.port,rsp.level);
            rsp.result = TRACKER_RSP_RESULT_OK;
        }
    }

    buffer_eat(c->rb, sizeof(f2t_addr_req_v3));

    // encode outer proto header
    uint32_t total_sz_outer = sizeof(proto_header) + sizeof(mt2t_proto_header) +
                        sizeof(proto_header) + sizeof(f2t_addr_rsp_v3);

    if (encode_header_v3(c->wb, CMD_MT2T_REQ_RSP, total_sz_outer) != 0)
    {
        ERR("tracker_process_addr_req_v3 encode outer protocol header error.");
        return -1;
    }

    // encode tracker header
    if (encode_mt2t_header(c->wb, tracker_header) != 0)
    {
        ERR("tracker_process_addr_req_v3 encode tracker header error.");
        return -1;
    }

    // encode inner proto header
    uint32_t total_sz_inner = sizeof(proto_header) + sizeof(f2t_addr_rsp_v3);

    if (encode_header_v3(c->wb, CMD_FC2T_ADDR_RSP_V3, total_sz_inner) != 0)
    {
        ERR("tracker_process_addr_req_v3 encode inner protocol header error.");
        return -1;
    }

    // encode payload
    if (encode_f2t_addr_rsp_v3(&rsp, c->wb) != 0)
    {
        ERR("tracker_process_addr_req_v3 encode_f2t_addr_rsp_v3 error.");
        return -1;
    }

    enable_forward_write(c);
    DBG("tracker encode rsp mt2t, seq=%d", int(tracker_header.seq));
    return 0;
}

static int
tracker_process_update_stream_map_v3(client_t *c)
{
    // decode outer proto header and tracker header
    proto_header pro_header_outer;
    mt2t_proto_header tracker_header;

    if (decode_f2t_header_v3(c->rb, &pro_header_outer, &tracker_header) != 0)
    {
        ERR("decode outer protocol header error when parsing update stream req v3.");
        return -1;
    }

    if (buffer_data_len(c->rb) < pro_header_outer.size)
    {
        ERR("invalid package found when parsing update stream req v3.");
        return -1;
    }

    if (CMD_MT2T_REQ_RSP != pro_header_outer.cmd)
        return -1;

    buffer_eat(c->rb, sizeof(proto_header) + sizeof(mt2t_proto_header));
    DBG("tracker process mt2t, seq=%d", int(tracker_header.seq));

    // decode inner proto header
    proto_header pro_header_inner;
    if (decode_header(c->rb, &pro_header_inner) != 0)
    {
        ERR("decode inner protocol header error when parsing update stream req v3.");
        return -1;
    }

    if (buffer_data_len(c->rb) < pro_header_inner.size)
    {
        ERR("length of request packet is not enough when parsing update stream req v3.");
        return -1;
    }

    if (CMD_FS2T_UPDATE_STREAM_REQ_V3 != pro_header_inner.cmd)
        return -1;

    buffer_eat(c->rb, sizeof(proto_header));

    //decode payload
    f2t_update_stream_req_v3 req;
    f2t_update_stream_rsp_v3 rsp;
    if (! decode_f2t_update_stream_req_v3(&req, c->rb))
    {
        WRN("%s",  "bad request packet of f2t update stream");
        rsp.result = TRACKER_RSP_RESULT_BADREQ;
    }
    else
    {
        string log_streamid;
        forward_manager->convert_streamid_array_to_string(req.streamid, log_streamid);

        INF("updating stream map v3: streamid:%s", log_streamid.c_str());
        rsp.result = TRACKER_RSP_RESULT_OK;
        if (req.cmd == UPD_CMD_ADD)
        {
            g_tracker_stat->add_stream_cnt++;
            // use level from configure file if there exist
            uint16_t asn, region;
            uint8_t level = -1;
            if (0 != tc_get_asn_and_region_and_level(c->ip, &asn, &region, &level))
            {
                WRN("found unrecognizing forward request v3. ip: %s",
                    util_ip2str(c->ip));
                return -1;
            }

            if (0 != forward_manager->fm_add_to_stream_v3(c->fid, &req, region)) // req.level is run time level
            {
                ERR("fm_add_to_stream error v3. streamid: %s ; fid: %lu",
                    log_streamid.c_str(), c->fid);
                rsp.result = TRACKER_RSP_RESULT_BADREQ;
            }
            else
            {
                INF("fm_add_to_stream succ v3. streamid:%s ; fid: %lu, ip: %s, req.level: %d",
                        log_streamid.c_str(), c->fid, util_ip2str(c->ip), req.level);
            }
        }
        else if (req.cmd == UPD_CMD_DEL)
        {
            g_tracker_stat->del_stream_cnt++;

            if (0 != forward_manager->fm_del_from_stream_v3(c->fid, &req))
            {
                ERR("fm_del_from_stream error v3. streamid: %s",
                    log_streamid.c_str());
                rsp.result = TRACKER_RSP_RESULT_BADREQ;
            }
            else
            {
                INF("fm_del_from_stream succ v3. streamid: %s; fid: %lu, ip:%s",
                        log_streamid.c_str(), c->fid, util_ip2str(c->ip));
            }
        }
        else
        {
            rsp.result = TRACKER_RSP_RESULT_BADREQ;
            ERR("fm_add_del_from_stream error v3. bad command: %u", req.cmd);
        }
        INF("stream map update done v3. streamid: %s", log_streamid.c_str());
    }

    buffer_eat(c->rb, sizeof(f2t_update_stream_req_v3));

    // encode outer proto header
    uint32_t total_sz_outer = sizeof(proto_header) + sizeof(mt2t_proto_header) +
                        sizeof(proto_header) + sizeof(f2t_update_stream_rsp_v3);

    if (encode_header_v3(c->wb, CMD_MT2T_REQ_RSP, total_sz_outer) != 0)
    {
        ERR("tracker_process_update_stream_map_v3 encode outer protocol header error.");
        return -1;
    }

    // encode tracker header
    if (encode_mt2t_header(c->wb, tracker_header) != 0)
    {
        ERR("tracker_process_update_stream_map_v3 encode tracker header error.");
        return -1;
    }

    // encode inner proto header
    uint32_t total_sz_inner = sizeof(proto_header) + sizeof(f2t_update_stream_rsp_v3);

    if (encode_header_v3(c->wb, CMD_FS2T_UPDATE_STREAM_RSP_V3, total_sz_inner) != 0)
    {
        ERR("tracker_process_update_stream_map_v3 encode inner protocol header error.");
        return -1;
    }

    // encode payload
    if (encode_f2t_update_stream_rsp_v3(&rsp, c->wb) != 0)
    {
        ERR("tracker_process_update_stream_map_v3 encode_f2t_update_rsp_v3 error.");
        return -1;
    }

    enable_forward_write(c);
    DBG("tracker encode rsp mt2t, seq=%d", int(tracker_header.seq));
    return 0;
}

static int
tracker_process_keep_alive_v3(client_t *c)
{
    // decode outer proto header and tracker header
    proto_header pro_header_outer;
    mt2t_proto_header tracker_header;

    if (decode_f2t_header_v3(c->rb, &pro_header_outer, &tracker_header) != 0)
    {
        ERR("decode outer protocol header error when parsing keep alive req v3.");
        return -1;
    }

    if (buffer_data_len(c->rb) < pro_header_outer.size)
    {
        ERR("invalid package found when parsing keep alive req v3.");
        return -1;
    }

    if (CMD_MT2T_REQ_RSP != pro_header_outer.cmd)
        return -1;

    buffer_eat(c->rb, sizeof(proto_header) + sizeof(mt2t_proto_header));
    DBG("tracker process mt2t, seq=%d", int(tracker_header.seq));

    // decode inner proto header
    proto_header pro_header_inner;
    if (decode_header(c->rb, &pro_header_inner) != 0)
    {
        ERR("decode inner protocol header error when parsing keep alive req v3.");
        return -1;
    }

    if (buffer_data_len(c->rb) < pro_header_inner.size)
    {
        ERR("length of request packet is not enough when parsing keep alive req v3.");
        return -1;
    }

    if (CMD_FS2T_KEEP_ALIVE_REQ_V3 != pro_header_inner.cmd)
        return -1;

    buffer_eat(c->rb, sizeof(proto_header));

    //decode payload
    f2t_keep_alive_req_v3 req;
    f2t_keep_alive_rsp_v3 rsp;
    if (! decode_f2t_keep_alive_req_v3(&req, c->rb))
    {
        rsp.result = TRACKER_RSP_RESULT_BADREQ;
        return -1;
    }
    else
    {
        if (0 == forward_manager->fm_active_forward(c->fid))
        {
            rsp.result = TRACKER_RSP_RESULT_OK;
        }
        else
        {
            // forward id not found, close connection directly
            ERR("not find the forward that keeping v3. fid: %lu", c->fid);
            return -1;
        }
    }

    buffer_eat(c->rb, sizeof(f2t_keep_alive_req_v3));

    // encode outer proto header
    uint32_t total_sz_outer = sizeof(proto_header) + sizeof(mt2t_proto_header) +
                        sizeof(proto_header) + sizeof(f2t_keep_alive_rsp_v3);

    if (encode_header_v3(c->wb, CMD_MT2T_REQ_RSP, total_sz_outer) != 0)
    {
        ERR("tracker_process_keep_alive_v3 encode outer protocol header error.");
        return -1;
    }

    // encode tracker header
    if (encode_mt2t_header(c->wb, tracker_header) != 0)
    {
        ERR("tracker_process_keep_alive_v3 encode tracker header error.");
        return -1;
    }

    // encode inner proto header
    uint32_t total_sz_inner = sizeof(proto_header) + sizeof(f2t_keep_alive_rsp_v3);

    if (encode_header_v3(c->wb, CMD_FS2T_KEEP_ALIVE_RSP_V3, total_sz_inner) != 0)
    {
        ERR("tracker_process_keep_alive_v3 encode inner protocol header error.");
        return -1;
    }

    // encode payload
    if (encode_f2t_keep_alive_rsp_v3(&rsp, c->wb) != 0)
    {
        ERR("tracker_process_keep_alive_v3 encode_f2t_keep_alive_rsp_v3 error.");
        return -1;
    }

    enable_forward_write(c);
    DBG("a forward is keeping v3. fid: %ld, ip: %s.", c->fid, util_ip2str(c->ip));
    DBG("tracker encode rsp mt2t, seq=%d", int(tracker_header.seq));
    return 0;
}

static int
tracker_process_server_stat(client_t *c)
{
    // decode outer proto header and tracker header
    proto_header pro_header_outer;
    mt2t_proto_header tracker_header;

    if (decode_f2t_header_v3(c->rb, &pro_header_outer, &tracker_header) != 0)
    {
        ERR("decode outer protocol header error when tracker_process_server_stat.");
        return -2;
    }

    if (buffer_data_len(c->rb) < pro_header_outer.size)
    {
        ERR("invalid package found when tracker_process_server_stat.");
        return -2;
    }

    if (CMD_MT2T_REQ_RSP != pro_header_outer.cmd)
        return -2;

    buffer_eat(c->rb, sizeof(proto_header) + sizeof(mt2t_proto_header));
    DBG("tracker process mt2t, seq=%d", int(tracker_header.seq));

    // decode inner proto header
    proto_header pro_header_inner;
    if (decode_header(c->rb, &pro_header_inner) != 0)
    {
        ERR("decode inner protocol header error when tracker_process_server_stat.");
        return -2;
    }

    if (buffer_data_len(c->rb) < pro_header_inner.size)
    {
        ERR("length of request packet is not enough when tracker_process_server_stat.");
        return -2;
    }

    if (CMD_IC2T_SERVER_STAT_REQ != pro_header_inner.cmd)
        return -2;

    buffer_eat(c->rb, sizeof(proto_header));

    //decode payload
    f2t_server_stat_rsp rsp;

    //int recvbuf_len = pro_header_inner.size - PROTO_HEADER_LEN;
    //char *recvbuf = new char[recvbuf_len + 1];
    //strcpy(recvbuf, buffer_data_ptr(c->rb));
    //recvbuf[recvbuf_len] = '\0';
    //InterliveServerStat p;
    //std::string data;
    //data.assign(buffer_data_ptr(c->rb), recvbuf_len);
    //bool ret = p.ParseFromString(data);

    int recvbuf_len = pro_header_inner.size - PROTO_HEADER_LEN;

    InterliveServerStat p;
    bool ret = p.ParseFromArray(buffer_data_ptr(c->rb), recvbuf_len);

    if (!ret)
    {
        rsp.result = TRACKER_RSP_RESULT_BADREQ;
        ERR("TRACKER_RSP_RESULT_BADREQ. fid: %lu", c->fid);
        return -2;
    }
    else
    {
        if (0 == forward_manager->fm_update_forward_server_stat(c->fid, p))
        {
            rsp.result = TRACKER_RSP_RESULT_OK;
            DBG("TRACKER_RSP_RESULT_OK. fid: %lu", c->fid);
        }
        else
        {
            rsp.result = TRACKER_RSP_RESULT_NORESULT;
            ERR("TRACKER_RSP_RESULT_NORESULT. fid: %lu", c->fid);
        }
    }

    //delete[] recvbuf;
    //recvbuf = NULL;
    buffer_eat(c->rb, recvbuf_len);

    // encode outer proto header
    uint32_t total_sz_outer = sizeof(proto_header) + sizeof(mt2t_proto_header) +
                        sizeof(proto_header) + sizeof(f2t_server_stat_rsp);

    if (encode_header_v3(c->wb, CMD_MT2T_REQ_RSP, total_sz_outer) != 0)
    {
        ERR("tracker_process_server_stat encode outer protocol header error.");
        return -2;
    }

    // encode tracker header
    if (encode_mt2t_header(c->wb, tracker_header) != 0)
    {
        ERR("tracker_process_server_stat encode tracker header error.");
        return -2;
    }

    // encode inner proto header
    uint32_t total_sz_inner = sizeof(proto_header) + sizeof(f2t_server_stat_rsp);

    if (encode_header_v3(c->wb, CMD_IC2T_SERVER_STAT_RSP, total_sz_inner) != 0)
    {
        ERR("tracker_process_server_stat encode inner protocol header error.");
        return -2;
    }

    // encode payload
    if (encode_f2t_server_stat_rsp(&rsp, c->wb) != 0)
    {
        ERR("tracker_process_server_stat encode_f2t_server_stat_rsp error.");
        return -2;
    }

    enable_forward_write(c);
    DBG("tracker_process_server_stat. fid: %ld, ip: %s.", c->fid, util_ip2str(c->ip));
    DBG("tracker encode rsp mt2t, seq=%d", int(tracker_header.seq));
    return 0;
}

static int
tracker_process_register_v4(client_t *c)
{
    // decode outer proto header and tracker header
    proto_header pro_header_outer;
    mt2t_proto_header tracker_header;

    if (decode_f2t_header_v4(c->rb, &pro_header_outer, &tracker_header) != 0)
    {
        ERR("decode outer protocol header error when parsing registering req v4.");
        return -1;
    }

    if (buffer_data_len(c->rb) < pro_header_outer.size)
    {
        ERR("invalid package found when parsing registering req v4.");
        return -1;
    }

    if (CMD_MT2T_REQ_RSP != pro_header_outer.cmd)
        return -1;

    buffer_eat(c->rb, sizeof(proto_header) + sizeof(mt2t_proto_header));
    DBG("tracker process mt2t, seq=%d", int(tracker_header.seq));

    // decode inner proto header
    proto_header pro_header_inner;
    if (decode_header(c->rb, &pro_header_inner) != 0)
    {
        ERR("decode inner protocol header error when parsing registering req v4.");
        return -1;
    }

    if (buffer_data_len(c->rb) < pro_header_inner.size)
    {
        ERR("length of request packet is not enough when parsing registering req v4.");
        return -1;
    }

    if (CMD_FS2T_REGISTER_REQ_V4 != pro_header_inner.cmd)
        return -1;

    buffer_eat(c->rb, sizeof(proto_header));

    // decode payload
    f2t_register_rsp_v3 rsp;

    int recvbuf_len = pro_header_inner.size - PROTO_HEADER_LEN;

    f2t_register_req_v4 req;
    bool ret = req.ParseFromArray(buffer_data_ptr(c->rb), recvbuf_len);
    if (!ret)
    {
        rsp.result = TRACKER_RSP_RESULT_BADREQ;
    }
    else
    {
        req.set_ip(ntohl(req.ip()));

        INF("a client registering v4: %s, %u, %u, %u, %u", util_ip2str(req.ip()), \
                req.backend_port(), req.player_port(), req.asn(), req.region());
        for (int i=0; i<req.public_ip_size(); i++)
        {

            INF("a client registering v4: public_ip_%d: %s", i, util_ip2str(req.public_ip(i)));
        }

        for (int i=0; i<req.private_ip_size(); i++)
        {

            INF("a client registering v4: private_ip_%d: %s", i, util_ip2str(req.private_ip(i)));
        }
        uint16_t asn=-1, region=-1;
        uint8_t level=-1;
        uint32_t group_id = -1;

        if (0 != tc_get_asn_and_region_and_level(req.ip(), &asn, &region, &level))
        {
            WRN("found unrecognizing forward v4. ip: %s", util_ip2str(req.ip()));
            return -1;
        }

        if (0 != tc_get_group_id(req.ip(), &group_id))
        {
            WRN("found unrecognizing forward v4. ip: %s", util_ip2str(req.ip()));
            return -1;
        }

        c->ip = req.ip();
        req.set_asn(asn);
        req.set_region(region);
        c->fid = forward_manager->fm_add_a_forward_v4(&req, level, group_id);
        if (c->fid == uint64_t(-1))
        {
            ERR("register client error v4. ip: %s; port: %u",
                util_ip2str(req.ip()), req.player_port());
            rsp.result = TRACKER_RSP_RESULT_INNER_ERR;
        }
        else
        {
            INF("a client registered v4. fid: %lu; ip: %s; port: %u; topo_level: %u",
                    c->fid, util_ip2str(req.ip()), req.player_port(), level);
            rsp.result = TRACKER_RSP_RESULT_OK;
            g_clients.push_back(c);

            /* add aliases if there exist */
            const char * alias = tc_get_alias(req.ip());
            if (alias)
            {
                const char * s = alias, *e = alias;
                while ((e = strchr(s, ';')) != NULL)
                {
                    char str_ip[32] = {0};
                    uint32_t ip;
                    uint16_t asn=-1, region=-1;
                    if (3 != sscanf(s, "%[^:]:%hu:%hu", str_ip, &asn, &region))
                    {
                        WRN("parse alias error.");
                        break;
                    }
                    INF("add alias v4: ip: %s, asn: %u, region: %u", str_ip, asn, region);
                    if (0 != util_str2ip(str_ip, &ip))
                    {
                        ERR("convert string ip to int ip error.");
                        break;
                    }
                    forward_manager->fm_add_alias(c->fid, ip, asn, region);
                    s = e+1;
                    if (*s == '\0')
                        break;
                }
            }
        }
    }

    buffer_eat(c->rb, recvbuf_len);

    // encode outer proto header
    uint32_t total_sz_outer = sizeof(proto_header) + sizeof(mt2t_proto_header) +
                        sizeof(proto_header) + sizeof(f2t_register_rsp_v3);

    if (encode_header_v3(c->wb, CMD_MT2T_REQ_RSP, total_sz_outer) != 0)
    {
        ERR("tracker_process_register_v4 encode outer protocol header error.");
        return -1;
    }

    // encode tracker header
    if (encode_mt2t_header(c->wb, tracker_header) != 0)
    {
        ERR("tracker_process_register_v4 encode tracker header error.");
        return -1;
    }

    // encode inner proto header
    uint32_t total_sz_inner = sizeof(proto_header) + sizeof(f2t_register_rsp_v3);

    if (encode_header_v3(c->wb, CMD_FS2T_REGISTER_RSP_V3, total_sz_inner) != 0)
    {
        ERR("tracker_process_register_v4 encode inner protocol header error.");
        return -1;
    }

    // encode payload
    if (encode_f2t_register_rsp_v3(&rsp, c->wb) != 0)
    {
        ERR("tracker_process_register_v4 encode_f2t_register_rsp_v3 error.");
        return -1;
    }

    enable_forward_write(c);
    DBG("tracker encode rsp mt2t, seq=%d", int(tracker_header.seq));
    return 0;
}

static int
tracker_process_r2p_keepalive_mt2t(client_t* c)
{
    // First header should contains CMD_MT2T_REQ_RSP
    proto_header old_header;
    // Second header contains a seq num
    mt2t_proto_header mt2t_header;
    // Third header should contains CMD_R2P_KEEPALIVE
    proto_header third_header;
    // Here we assume r2p_keepalive will use same format with f2t_xxxxx
    if (decode_f2t_header_v3(c->rb, &old_header, &mt2t_header) != 0)
    {
        ERR("tracker_process_r2p_keepalive_mt2t: decode_f2t_header_v3 error");
        return -1;
    }
    DBG("tracker_process_r2p_keepalive_mt2t seq=%d", int(mt2t_header.seq));
    if (decode_header(c->rb, &third_header))
    {
        ERR("tracker_process_r2p_keepalive_mt2t: decode_header error");
        return -1;
    }
    buffer_eat(c->rb, sizeof(proto_header) + sizeof(mt2t_proto_header) + sizeof(proto_header));

    // other fields of keepalive are ignored since we merged keepalive messages into a single one
    // if this caused a problem, we should simply pass each keepalive message to portal
    r2p_keepalive* keepalive = (r2p_keepalive*)(buffer_data_ptr(c->rb));
    size_t stream_count = ntohl(keepalive->stream_cnt);
    DBG("tracker_process_r2p_keepalive_mt2t: got %d stream(s)", stream_count);
    buffer_eat(c->rb, sizeof(r2p_keepalive));
    for (size_t i = 0; i < stream_count; i++)
    {
        receiver_stream_status* stream_status = (receiver_stream_status*) buffer_data_ptr(c->rb);
        uint32_t stream_id = ntohl(stream_status->streamid);
        g_stream_manager->update_if_existed(stream_id, ntohl(stream_status->last_ts),
                                            util_ntohll(stream_status->block_seq));
        buffer_eat(c->rb, sizeof(receiver_stream_status));
    }
    // Encode 3 headers here as required by module_tracker
    size_t response_total_bytes = sizeof(proto_header) + sizeof(mt2t_proto_header)
                                + sizeof(proto_header);
    if (encode_header_v3(c->wb, CMD_MT2T_REQ_RSP, response_total_bytes))
    {
        ERR("tracker_process_r2p_keepalive_mt2t: encode_header_v3 error");
        return -1;
    }

    if (encode_mt2t_header(c->wb, mt2t_header))
    {
        ERR("tracker_process_r2p_keepalive_mt2t: encode_mt2t_header error");
        return -1;
    }

    if (encode_header_v3(c->wb, CMD_P2R_KEEPALIVE_ACK, sizeof(proto_header)))
    {
        ERR("tracker_process_r2p_keepalive_mt2t: 2nd encode_header_v3 error");
        return -1;
    }

    // The P2R_KEEPALIVE_ACK is empty, dont encode anything
    enable_forward_write(c);
    return 0;
}

static int
tracker_process_f2p_white_list_req_mt2t(client_t* c)
{
    // First header should contains CMD_MT2T_REQ_RSP
    proto_header old_header;
    // Second header contains a seq num
    mt2t_proto_header mt2t_header;
    // Third header should contains CMD_R2P_KEEPALIVE
    proto_header third_header;
    // Here we assume r2p_keepalive will use same format with f2t_xxxxx
    if (decode_f2t_header_v3(c->rb, &old_header, &mt2t_header) != 0)
    {
        ERR("tracker_process_f2p_white_list_req_mt2t: decode_f2t_header_v3 error");
        return -1;
    }

    DBG("tracker_process_f2p_white_list_req_mt2t seq=%d", int(mt2t_header.seq));

    if (decode_header(c->rb, &third_header))
    {
        ERR("tracker_process_f2p_white_list_req_mt2t: decode_header error");
        return -1;
    }
    buffer_eat(c->rb, sizeof(proto_header) + sizeof(mt2t_proto_header) + sizeof(proto_header));

    // Refresh the buffer if stale
    if (g_stream_list_buffer_stale)
    {
        tracker_update_stream_list_buffer();
        g_stream_list_buffer_stale = false;
    }

    // Encode 3 headers here as required by module_tracker
    size_t response_total_bytes = sizeof(proto_header) + sizeof(mt2t_proto_header)
                                  + sizeof(proto_header) + buffer_data_len(tracker_get_stream_list_buffer());
    if (encode_header_v3(c->wb, CMD_MT2T_REQ_RSP, response_total_bytes))
    {
        ERR("tracker_process_f2p_white_list_req_mt2t: encode_header_v3 error");
        return -1;
    }

    if (encode_mt2t_header(c->wb, mt2t_header))
    {
        ERR("tracker_process_f2p_white_list_req_mt2t: encode_mt2t_header error");
        return -1;
    }

    if (encode_header_v3(c->wb, CMD_P2F_WHITE_LIST_RSP, sizeof(proto_header) + buffer_data_len(tracker_get_stream_list_buffer())))
    {
        ERR("tracker_process_f2p_white_list_req_mt2t: 2nd encode_header_v3 error");
        return -1;
    }

    // Encode the final response
    buffer_append(c->wb, tracker_get_stream_list_buffer());
    enable_forward_write(c);
    return 0;
}

static int
tracker_process_f2p_keepalive_mt2t(client_t* c)
{
    // First header should contains CMD_MT2T_REQ_RSP
    proto_header old_header;
    // Second header contains a seq num
    mt2t_proto_header mt2t_header;
    // Third header should contains CMD_R2P_KEEPALIVE
    proto_header third_header;
    // Here we assume r2p_keepalive will use same format with f2t_xxxxx
    if (decode_f2t_header_v3(c->rb, &old_header, &mt2t_header) != 0)
    {
        ERR("tracker_process_f2p_keepalive_mt2t: decode_f2t_header_v3 error");
        return -1;
    }

    DBG("tracker_process_f2p_keepalive_mt2t seq=%d", int(mt2t_header.seq));

    if (decode_header(c->rb, &third_header))
    {
        ERR("tracker_process_f2p_keepalive_mt2t: decode_header error");
        return -1;
    }
    buffer_eat(c->rb, sizeof(proto_header) + sizeof(mt2t_proto_header) + sizeof(proto_header));

    f2p_keepalive* keepalive = (f2p_keepalive*)(buffer_data_ptr(c->rb));
    size_t stream_count = htonl(keepalive->stream_cnt);
    buffer_eat(c->rb, sizeof(f2p_keepalive));
    buffer_eat(c->rb, sizeof(forward_stream_status) * stream_count);

    // Encode 3 headers here as required by module_tracker
    size_t response_total_bytes = sizeof(proto_header) + sizeof(mt2t_proto_header)
                                + sizeof(proto_header);
    if (encode_header_v3(c->wb, CMD_MT2T_REQ_RSP, response_total_bytes))
    {
        ERR("tracker_process_f2p_keepalive_mt2t: encode_header_v3 error");
        return -1;
    }

    if (encode_mt2t_header(c->wb, mt2t_header))
    {
        ERR("tracker_process_f2p_keepalive_mt2t: encode_mt2t_header error");
        return -1;
    }

    if (encode_header_v3(c->wb, CMD_P2F_KEEPALIVE_ACK, sizeof(proto_header)))
    {
        ERR("tracker_process_f2p_keepalive_mt2t: 2nd encode_header_v3 error");
        return -1;
    }

    // The P2R_KEEPALIVE_ACK is empty, dont encode anything
    enable_forward_write(c);
    return 0;
}

static int
tracker_process_st2t_fpinfo_req_mt2t(client_t* c)
{
    static buffer* fpinfo_buffer = buffer_init(4096);

    // First header should contains CMD_MT2T_REQ_RSP
    proto_header old_header;
    // Second header contains a seq num
    mt2t_proto_header mt2t_header;
    // Third header should contains CMD_R2P_KEEPALIVE
    proto_header third_header;
    // Here we assume r2p_keepalive will use same format with f2t_xxxxx
    if (decode_f2t_header_v3(c->rb, &old_header, &mt2t_header) != 0)
    {
        ERR("tracker_process_st2t_fpinfo_req_mt2t: decode_f2t_header_v3 error");
        return -1;
    }

    DBG("tracker_process_st2t_fpinfo_req_mt2t seq=%d", int(mt2t_header.seq));

    if (decode_header(c->rb, &third_header))
    {
        ERR("tracker_process_st2t_fpinfo_req_mt2t: decode_header error");
        return -1;
    }
    buffer_eat(c->rb, sizeof(proto_header) + sizeof(mt2t_proto_header) + sizeof(proto_header));

    st2t_fpinfo_packet& fpinfos = forward_manager->fm_get_forward_fp_infos();
    size_t bytes_to_serialize = fpinfos.ByteSize();

    if (buffer_size(fpinfo_buffer) < bytes_to_serialize)
    {
        size_t new_size = buffer_size(fpinfo_buffer);
        while (new_size < bytes_to_serialize)
        {
            new_size *= 2;
        }
        buffer_expand(fpinfo_buffer, new_size);
    }
    else if (buffer_size(fpinfo_buffer) > 2 * bytes_to_serialize)
    {
        // do nothing
    }

    buffer_reset(fpinfo_buffer);
    bool serialized = fpinfos.SerializeToArray(const_cast<char*>(buffer_data_ptr(fpinfo_buffer)), bytes_to_serialize);
    if (!serialized)
    {
        ERR("tracker_process_st2t_fpinfo_req_mt2t error: failed to serialize protobuf, fid = %lu", c->fid);
        return -1;
    }
    fpinfo_buffer->_end += bytes_to_serialize;
    // Encode 3 headers here as required by module_tracker
    size_t response_total_bytes = sizeof(proto_header) + sizeof(mt2t_proto_header)
                                  + sizeof(proto_header) + bytes_to_serialize;
    if (encode_header_v3(c->wb, CMD_MT2T_REQ_RSP, response_total_bytes))
    {
        ERR("tracker_process_st2t_fpinfo_req_mt2t: encode_header_v3 error");
        return -1;
    }

    if (encode_mt2t_header(c->wb, mt2t_header))
    {
        ERR("tracker_process_st2t_fpinfo_req_mt2t: encode_mt2t_header error");
        return -1;
    }

    if (encode_header_v3(c->wb, CMD_T2ST_FPINFO_RSP, sizeof(proto_header) + bytes_to_serialize))
    {
        ERR("tracker_process_st2t_fpinfo_req_mt2t: 2nd encode_header_v3 error");
        return -1;
    }

    if (buffer_append(c->wb, fpinfo_buffer) != 0)
    {
        ERR("tracker_process_st2t_fpinfo_req_mt2t error: failed to append CMD_T2ST_FPINFO_RSP to write buffer, fid = %lu", c->fid);
        return -1;
    }

    enable_forward_write(c);
    return 0;
}

buffer* tracker_get_stream_list_buffer()
{
    static buffer* stream_list_buffer = buffer_init(4096);
    return stream_list_buffer;
}

buffer* tracker_prepare_stream_list_buffer(size_t bytes)
{
    buffer* stream_list_buffer = tracker_get_stream_list_buffer();
    // expanding the buffer
    if (buffer_size(stream_list_buffer) < bytes)
    {
        size_t new_buffer_bytes = buffer_size(stream_list_buffer);
        while (new_buffer_bytes < bytes)
        {
            new_buffer_bytes *= 2;
        }
        buffer_expand(stream_list_buffer, (int)new_buffer_bytes);
    }
    // shrinking the buffer
    else if (bytes < buffer_size(stream_list_buffer) / 2)
    {
        // do nothing
    }
    buffer_reset(stream_list_buffer);
    return stream_list_buffer;
}

void tracker_update_stream_list_buffer()
{
    uint16_t stream_count = g_stream_manager->stream_map().size();
    uint32_t total_bytes = sizeof(p2f_inf_stream_v2)
                           + sizeof(stream_info) * stream_count;

    buffer* stream_list_buffer = (buffer*) tracker_prepare_stream_list_buffer(total_bytes);

    uint16_t bigendian_stream_count = htons(stream_count);
    buffer_append_ptr(stream_list_buffer, &bigendian_stream_count, sizeof(uint16_t));
    for (StreamMap::iterator it = g_stream_manager->stream_map().begin();
         it != g_stream_manager->stream_map().end();
         it++)
    {
        StreamItem& si = it->second;
        uint32_t bigendian_stream_id = htonl(si.stream_id);
        uint64_t bigendian_tv_sec = util_htonll(si.start_time.tv_sec);
        uint64_t bigendian_tv_usec = util_htonll(si.start_time.tv_usec);
        buffer_append_ptr(stream_list_buffer, &bigendian_stream_id, sizeof(uint32_t));
        buffer_append_ptr(stream_list_buffer, &bigendian_tv_sec, sizeof(uint64_t));
        buffer_append_ptr(stream_list_buffer, &bigendian_tv_usec, sizeof(uint64_t));
    }
}

void tracker_broadcast_stream_close(const uint32_t stream_id)
{
    static mt2t_proto_header mt2t_header = {0, 0};
    size_t response_total_bytes = sizeof(proto_header) + sizeof(mt2t_proto_header)
                                  + sizeof(proto_header) + sizeof(p2f_close_stream);
    static buffer* stream_close_header = NULL;
    if (!stream_close_header) {
        stream_close_header = buffer_init(sizeof(proto_header) + sizeof(mt2t_proto_header) + sizeof(proto_header));
        encode_header_v3(stream_close_header, CMD_MT2T_REQ_RSP, response_total_bytes);
        encode_mt2t_header(stream_close_header, mt2t_header);
        encode_header_v3(stream_close_header, CMD_P2F_CLOSE_STREAM, sizeof(proto_header) + sizeof(p2f_close_stream));
    }

    p2f_close_stream close_stream;
    close_stream.streamid = htonl(stream_id);

    for (client_list_t::iterator it = g_clients.begin(); it != g_clients.end(); it++)
    {
        client_t* client = *it;
        buffer_append(client->wb, stream_close_header);
        buffer_append_ptr(client->wb, &close_stream, sizeof(p2f_close_stream));
        enable_forward_write(client);
    }
}

void tracker_broadcast_stream_start(const StreamItem& item)
{
    static mt2t_proto_header mt2t_header = {0, 0};
    size_t response_total_bytes = sizeof(proto_header) + sizeof(mt2t_proto_header)
                                  + sizeof(proto_header) + sizeof(p2f_start_stream_v2);
    static buffer* stream_start_header = NULL;
    if (!stream_start_header) {
        stream_start_header = buffer_init(sizeof(proto_header) + sizeof(mt2t_proto_header) + sizeof(proto_header));
        encode_header_v3(stream_start_header, CMD_MT2T_REQ_RSP, response_total_bytes);
        encode_mt2t_header(stream_start_header, mt2t_header);
        encode_header_v3(stream_start_header, CMD_P2F_START_STREAM_V2, sizeof(proto_header) + sizeof(p2f_start_stream_v2));
    }

    p2f_start_stream_v2 start_stream_v2;
    start_stream_v2.streamid = htonl(item.stream_id);
    start_stream_v2.start_time.tv_sec = util_htonll(item.start_time.tv_sec);
    start_stream_v2.start_time.tv_usec = util_htonll(item.start_time.tv_usec);

    for (client_list_t::iterator it = g_clients.begin(); it != g_clients.end(); it++)
    {
        client_t* client = *it;
        buffer_append(client->wb, stream_start_header);
        buffer_append_ptr(client->wb, &start_stream_v2, sizeof(p2f_start_stream_v2));
        enable_forward_write(client);
    }
}
