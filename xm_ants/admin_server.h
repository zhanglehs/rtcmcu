/*
 * a http server for:
 * 1, starting/stopping a stream ant
 * 2, get state of xm_ants
 *
 * author: hechao@youku.com
 * create: 2014/3/10
 *
 */

#ifndef __ADMIN_SERVER_H_
#define __ADMIN_SERVER_H_

#include <sys/time.h>
#include <string>
#include <map>

#include <utils/buffer_queue.h>

#include "http_connect.h"
#include "ant.h"
#include "levent.h"
#include "streamid.h"

#ifdef _WIN32
#define NULL 0
#endif

class FLVSender;

enum AntPairStatus
{
    ANT_PAIR_ACCEPT = 0,
    ANT_PAIR_STARTING,
    ANT_PAIR_RUNNING,
    ANT_PAIR_RESCHEDULED,
    ANT_PAIR_RESTARTING,
    ANT_PAIR_STOPED,
};

// CAUTIONS!!!
// stop AntPair MUST BE carefully!
class AntPair
{
public:
    enum SHUTDOWN_FLAG
    {
        FLAG_ANT_SHUTDOWN = 1,
        FLAG_SENDER_SHUTDOWN = 2
    };

    AntPair();

    ~AntPair();

    int check_restart();

    int check_reschedule();

    void heart_beat();

    void stop(bool reschedule);

    static int set_config(int tolerate_failure_time, int tolerate_failure_count);

    // the bits of shutdown_flag are changed by callback of ant or sender
    uint8_t shutdown_flag;
    uint8_t id_version;
    std::string rtmp_uri;
    std::vector<StreamId_Ext> _stream_id_vec;
    FLVSender *sender;
    lcdn::base::BufferQueue *buffer;
    int *stream_id_remove_flag;
    int stream_n_number;
    bool ant_start_flag;
    Ant *ant;

    string  type_string;
    string  uri;
    string  req_path;

    struct timeval  last_start_time;
    struct timeval  first_retry_time;
    struct timeval  plan_end_time;
    int restart_wait_time_us;

    static struct timeval   tolerate_failure_time;
    int             continuous_failure_count;
    static int      tolerate_failure_count;
    
    AntPairStatus status;

    bool reschedule_flag;
    bool plan_end_flag;

    enum RestartStrategy
    {
        RESTART_STRATEGY_STOP = 0,
        RESTART_STRATEGY_RESTART = 1,
        RESTART_STRATEGY_WAIT = 2,
        RESTART_STRATEGY_SUCCESS = 3,
    };
};

enum AntEvent
{
    ANT_EVENT_STARTED = 0,
    ANT_EVENT_STOPED,
};

enum SenderEvent
{
    SENDER_EVENT_STARTED = 0,
    SENDER_EVENT_STOPED
};

class AdminServer: public Getable, public Postable
{
    public:
        AdminServer(uint32_t listen_ip, uint16_t listen_port, int max_ant_count);
        virtual ~AdminServer();

        int init();
        void accept_handler(int fd, const short event);

        virtual int get(const char *req_path, lcdn::base::Buffer *result);
        virtual int post(const char *req_path, const char *req_body, lcdn::base::Buffer *result);

        void http_finish_callback(int fd);
        void set_event_base(struct event_base *base) { _event_base = base;}

        void ant_callback(const StreamId_Ext& streamid, AntEvent event, bool restart);
        void sender_callback(const StreamId_Ext& streamid, SenderEvent event, bool restart);
        void on_timer(const int fd, const short event);

        int  force_exit_all();

    private:
        int _start_an_ant(std::string& req_path, bool restart);
        int _stop_an_ant(StreamId_Ext& stream_id);

        int _http_start(std::string req_path, std::string& response);

        int _http_stop(std::string req_path, std::string& response);

        int _http_status(std::string req_path, std::string& response);

        int _http_test(std::string req_path, std::string& response);

        void _stop(std::map<StreamId_Ext, AntPair*>::iterator i);

        void _start_timer(uint32_t sec, uint32_t usec);

        string _build_status_info(string property);

    private:
        uint32_t _listen_ip;
        uint16_t _listen_port;
        int _listen_sock;
        int _max_ant_count;
        int _current_ant_count;

        std::map<int, HTTPConnection*> _connect_map;
        std::map<StreamId_Ext, AntPair *> _pair_map;
//      std::map<StreamId_Ext, ScheduledAntPair*> _scheduled_ant_map;

        struct event _listen_event;
        struct event _timer_event;
        struct event_base * _event_base;

        bool _force_exit_flag;
};

#endif

