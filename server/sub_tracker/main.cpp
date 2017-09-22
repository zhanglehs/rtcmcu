/*
 * main for tracker
 * author: hechao@youku.com
 * create: 2013.5.21
 *
 */

/*
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
*/
#include <signal.h>
#include <unistd.h>

#include <util/levent.h>
#include <util/log.h>
#include <util/util.h>
#include <util/daemon.h>
#include <util/ketama_hash.h>

#include "tracker.h"
#include "tracker_config.h"
#include "tracker_def.h"
#include "tracker_stat.h"
#include "http_server.h"
#include "version.h"

const char * tracker_version = TRACKER_VERSION;

static struct event_base * main_base = NULL;
extern tracker_config_t *g_config;

static void _signal_handler(int num)
{
    /* TODO
     * do clean work
     */

    _exit(0);
}

static void _set_signal_handler()
{
    signal(SIGHUP, _signal_handler);
    signal(SIGINT, _signal_handler);
    signal(SIGUSR1, _signal_handler);
    signal(SIGUSR2, _signal_handler);
    signal(SIGTERM, _signal_handler);
    signal(SIGCHLD, _signal_handler);

    /* ignore SIGPIPE when write to closing fd
     * otherwise EPIPE error will cause transporter to terminate unexpectedly
     */
    signal(SIGPIPE, SIG_IGN);

}

int main(int argc, char **argv)
{
    _set_signal_handler();

    //if (0 != tracker_config_init(g_config))
    if (0 != tracker_config_init())
    {
        fprintf(stderr, "tracker_config_init failed. \n");
        return 1;
    }
    if (0 != tracker_parse_arg(argc, argv))
    {
        fprintf(stderr, "argument parse failed. \n");
        return 1;
    }
    //if (0 != tracker_parse_config(g_config))
    if (0 != tracker_parse_config())
    {
        fprintf(stderr, "configure parse failed. \n");
        return 1;
    }
    int ret = log_init_file(g_config->log_dir, g_config->log_name, LOG_LEVEL_DBG, 100*1024*1024);
    if(0 != ret)
    {
        fprintf(stderr, "log init failed. ret = %d\n", ret);
        return 1;
    }
    log_set_level(g_config->log_level);

    if(0 != util_set_limit(RLIMIT_NOFILE, 40000, 40000))
    {
        ERR("set file no limit to 40000 failed.");
        return 1;
    }
#ifdef HAVE_SCHED_GETAFFINITY
    /*  TODO
    if(0 != util_unmask_cpu0())
    {
        ERR("unmask cpu0 failed.");
        return 1;
    }
    */
#endif

    //main_base = event_init();
    main_base = event_base_new();
    if (NULL == main_base)
    {
        ERR("can't init libevent");
        return 1;
    }
    levent_init();

    lcdn::net::EventLoop *loop = new lcdn::net::EventLoop(main_base);
    if (tracker_init(g_config) < 0
            || tracker_start_tracker(loop, main_base) < 0)
    {
        ERR("init tracker error.");
        return -1;
    }

    //tracker_start_console();

    /*simple tracker httpd*/
    if (tracker_start_http_server(main_base) < 0)
    {
        ERR("tracker http server start error.");
        return -1;
    }

    if (g_config->is_daemon)
        daemonize(1, 0);

    DBG("tracker begin event loop.");
    loop->loop();
    tracker_stop_tracker();

    return 0;
}

