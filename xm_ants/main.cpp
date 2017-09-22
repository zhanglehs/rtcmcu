/*
 * the main entry of xm_ants
 * author: hechao@youku.com
 * create: 2014/3/4
 *
 */
#include <string>
#include <iostream>
#include <signal.h>
#include <getopt.h>

#include <version.h>
#include <utils/util.h>
#include <utils/stacktrace.h>

#include "xmants.h"
#include "global_var.h"
#include "logger.h"
#include "admin_server.h"

//#include <google/heap-profiler.h>

using namespace log4cplus;
using namespace std;

XMAnts *ants = NULL;

const char* Process_name = "xm_ants";
bool to_daemon = false;

string config_file_name = "";

time_t process_start_time;

void sig_handler(int sig)
{
    if (ants != NULL)
    {
        ants->force_exit_all();
        LOG_INFO(g_logger, "main: ants exit by SIGTERM signal.");
    }
}

void signal_tragedy()
{
    struct sigaction sig;
    memset(&sig, 0, sizeof(sig));
    sigemptyset(&sig.sa_mask);
    sig.sa_flags = 0x0;
    sig.sa_handler = SIG_IGN;

//    sigaction(SIGPIPE, &sig, 0);
//    sigaction(SIGHUP, &sig, 0);
//    sigaction(SIGINT, &sig, 0);
//    sigaction(SIGQUIT, &sig, 0);
//    sigaction(SIGURG, &sig, 0);
//    sigaction(SIGTSTP, &sig, 0);
//    sigaction(SIGTTIN, &sig, 0);
//    sigaction(SIGTTOU, &sig, 0);
//    sigaction(SIGALRM, &sig, 0);
//    sigaction(SIGCHLD, &sig, 0);

    struct sigaction sig1;
    memset(&sig1, 0, sizeof(sig1));
    sigemptyset(&sig1.sa_mask);
    sig1.sa_flags = 0x0;
    sig1.sa_handler = sig_handler;

    sigaction(SIGTERM, &sig1, 0);
}

void show_help()
{
    cout<<"help info."<<endl;
}

void show_version()
{
    cout << "xm_ants" <<endl;
    cout << "version: " <<VERSION<< endl;
    cout << "build: " << BUILD << endl;
    cout<<"branch: "<< BRANCH<<endl;
    cout<<"hash: "<< HASH<<endl;
    cout<<"release date: "<< DATE<<endl;
    cout<<endl;
}

int parse_arg(int argc, char* argv[])
{
    static struct option long_options[] = {
        {"version",     no_argument,        NULL,   'v'},
        {"help",        no_argument,        NULL,   'h'},
        {"daemon",      no_argument,        NULL,   'd'},
        {"config-file", required_argument,  NULL,   'c'},
        {0,0,0,0}
    };

    int opt;
    int option_index = 0;
    char* optstring = "vhc:d";

    while( (opt = getopt_long(argc, argv, optstring, long_options, &option_index)) != -1)
    {
        switch (opt)
        {
            case 'c':
                config_file_name = optarg;
                break;

            case 'd':
                to_daemon = true;
                break;

            case 'h':
                show_help();
                exit(0);

            case 'v':
                show_version();
                exit(0);
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{ 
    process_start_time = time(NULL);
    parse_arg(argc, argv);

    signal_tragedy();

    ants = new XMAnts(config_file_name);
    ants->set_version( VERSION); 

    if (ants->init() != 0)
    {
        std::cerr << "XMAnts init failed." << std::endl;
        LOG_FATAL(g_logger, "main: XMAnts init failed.");
        return -1;
    }

    levent_init();
    g_event_base = event_base_new();
    if (!g_event_base)
    {
        LOG_FATAL(g_logger, "create event base error.");
        return -1;
    }
    ants->set_event_base(g_event_base);
    LOG_INFO(g_logger, "starting ants");
    if (ants->start() != 0)
    {
        LOG_FATAL(g_logger, "main: start ants error.");
        return -1;
    }

    if (to_daemon && daemon(true, 0) == -1)
    {
        std::cerr << "xm_ants failed to be a daemon." << std::endl;
        exit(1);
    }

    int ret = util_create_pid_file(".", Process_name);

    if (ret < 0)
    {
        LOG_FATAL(g_logger, "xm_ants failed to create pid file");
        return -1;
    }

    ret = stacktrace_init(".", "xm_ants", VERSION);
    if (ret < 0)
    {
        LOG_FATAL(g_logger, "xm_ants failed to create stacktrace file");
        return -1;
    }

//    HeapProfilerStart("/home/songshenyi/project/mirror_live_stream_svr/xm_ants/profiler/p");
//    printf("HeapProfilerStart\n");
    event_base_dispatch(g_event_base);

//    HeapProfilerStop();
//    printf("HeapProfilerStop\n");

    if (ants)
    {
        delete ants;
        ants = NULL;
    }

    return 0;
}

