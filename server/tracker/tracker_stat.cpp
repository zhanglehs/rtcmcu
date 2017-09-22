/*
 * author: hechao@youku.com
 * create: 2013.7.18
 */

#include "tracker_stat.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <utils/memory.h>
#include <util/statfile.h>
#include <util/log.h>
#include <util/util.h>

#include "tracker_config.h"
#include "tracker_def.h"

tracker_stat_t * g_tracker_stat = NULL;
extern tracker_config_t *g_config;

static int statfd = -1;

int
tracker_stat_init(const char *log_svr_ip, uint16_t log_svr_port)
{
    if (!g_tracker_stat)
    {
        g_tracker_stat = (tracker_stat_t *)mcalloc(1, sizeof(tracker_stat_t));
        if (!g_tracker_stat)
        {
            ERR("calloc tracker_stat_t struct failed.");
            return -1;
        }
    }

    //statfd = open_statfile_or_pipe("|/usr/bin/logger -p local5.info");
    char arg[64];
    memset(arg,0,sizeof(arg));
    snprintf(arg,sizeof(arg),"@%s:%hu", log_svr_ip, log_svr_port);
    statfd = open_statfile_or_pipe(arg);
    if (-1 == statfd)
    {
        ERR("open stat fd failed.");
        return -1;
    }

    return 0;
}

void tracker_stat_close()
{
    if (!g_tracker_stat)
    {
        mfree(g_tracker_stat);
        g_tracker_stat = NULL;
    }

    if (statfd != -1)
    {
        close(statfd);
        statfd = -1;
    }
}

void
tracker_stat_dump_and_reset(tracker_stat_t * stat)
{
    static char tmp[128];
    memset(tmp, 0, sizeof(tmp));

    struct tm now_time;
    time_t t = time(NULL);
    if(NULL == localtime_r(&t, &now_time)) {
        return;
    }
    /* TODO add other items to log */
    snprintf(tmp, sizeof(tmp), "%s:%u\tRPT\ttracker\t%04d-%02d-%02d %02d:%02d:%02d\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
            util_ip2str(ntohl(inet_addr(g_config->listen_ip))),
            g_config->listen_port,
            now_time.tm_year + 1900, now_time.tm_mon + 1, now_time.tm_mday,
            now_time.tm_hour, now_time.tm_min, now_time.tm_sec,
            //time(0),
            tracker_version,
            stat->cur_stream_num,
            stat->cur_forward_num,
            stat->register_cnt,
            stat->add_stream_cnt,
            stat->del_stream_cnt,
            stat->keep_req_cnt,
            stat->req_addr_cnt);

    ssize_t size = write(statfd, tmp, strlen(tmp));
    if (size < 0)
    {
        ERR("write log to log server error");
    }

    INF("%s", tmp);

    memset(stat, 0, sizeof(*stat));
}


