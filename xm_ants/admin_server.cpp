/*
 *
 * author: hechao@youku.com
 * create: 2014/3/11
 *
 */

#include <net/net_utils.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/uri.h>
#include <vector>
#include <sstream>
#include <time.h>

#include <json-c++/json.h>
#include  "streamid.h"

#include "rtmp_ant.h"
#include "flv_sender.h"
#include "global_var.h"
#include "mms2flv_ant.h"
#include "admin_server.h"
#include "http_server.h"

using namespace std;
using namespace lcdn;
using namespace lcdn::base;

#define foreach(container, it) \
    for (typeof((container).begin()) it = (container).begin(); it != (container).end(); ++it)

const static string METHOD_START = "start";
const static string METHOD_STOP = "stop";
const static string METHOD_STATUS = "status";
const static string METHOD_VERSION = "version";
const static string METHOD_TEST = "test";

const static int MAX_SUPPORT_STREAM_NUMBER = 5;

static void _accept_handler(int fd, const short event, void *arg)
{
    static_cast<AdminServer *>(arg)->accept_handler(fd, event);
}
static void timer_event_handler(const int fd, const short event, void *ctx)
{
    static_cast<AdminServer *>(ctx)->on_timer(fd, event);
}


AdminServer::AdminServer(uint32_t listen_ip, uint16_t listen_port, int max_ant_count)
    :_listen_ip(listen_ip), _listen_port(listen_port), _max_ant_count(max_ant_count),
    _current_ant_count(0), _force_exit_flag(false)
{
}

AdminServer::~AdminServer()
{
    for(map<int,HTTPConnection*>::iterator i = _connect_map.begin();
            i != _connect_map.end();
            i++)
    {
        delete (i->second);
        i->second = NULL;
    }
    close(this->_listen_sock);
    LOG_INFO(g_logger, "AdminServer: ~AdminServer().");
}

int AdminServer::init()
{
    // 1, bind a listening socket
    _listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (_listen_sock < 0)
    {
        return -1;
    }
    if (net_util_set_nonblock(_listen_sock, 1) < 0)
    {
        return -1;
    }

    int val = 1; 
    if (-1 == setsockopt(_listen_sock, SOL_SOCKET, SO_REUSEADDR, (void*) &val,
            sizeof(val)))
    {
        LOG_ERROR(g_logger, "set socket SO_REUSEADDR failed. error = " << strerror(errno));
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(_listen_ip);
    addr.sin_port = htons(_listen_port);

    if (bind(_listen_sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)))
    {
        LOG_ERROR(g_logger, "AdminServer: bind socket error, error:" << strerror(errno));
        return -1;
    }
    if (listen(_listen_sock, 0))
    {
        return -1;
    }

    levent_set(&_listen_event, _listen_sock, EV_READ | EV_PERSIST, _accept_handler, this);
    event_base_set(_event_base, &_listen_event);
    levent_add(&_listen_event, 0);

    _start_timer(0, 10000);

    return 0;
}

int AdminServer::_start_an_ant(string& req_path, bool restart)
{
    URI uri;
    uri.parse_path(req_path);
    char* token = NULL;

    string src_uri = uri.get_query("src");
    string src_type = uri.get_query("src_type");
    string dst_ip = uri.get_query("dst_host");
    int dst_port = atoi(uri.get_query("dst_port").c_str());
    time_t plan_duration = atoi(uri.get_query("et").c_str());
    string uuid_str = uri.get_query("dst_sid");

    if (src_uri == "" || src_type == "" || dst_ip == "" || dst_port == 0 || uuid_str == "")
    {
        LOG_ERROR(g_logger, "_start_an_ant parameter wrong!!! ");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    if(0 == inet_aton(dst_ip.c_str(), &addr.sin_addr))
    {
        LOG_ERROR(g_logger, "_start_an_ant parameter wrong!!! dst_ip = " << dst_ip);
        return -1;
    }

    assert(src_uri != "");
    assert(src_type != "");
    assert(dst_ip != "");
    assert(dst_port != 0);
    assert(uuid_str != "");

    LOG_INFO(g_logger, "_start_an_ant uuid_str in req_path is: " << uuid_str);

    //parse out stream numbers from url.
    char uuid_str_copy[164];
    vector<string> stream_id_string_array;
    int stream_counter = 0;

    strcpy(uuid_str_copy, uuid_str.c_str());
    token = strtok(uuid_str_copy, ",");
    
    while(token != NULL)
    {
        stream_counter++;
        stream_id_string_array.push_back(token);
        if (stream_counter >= MAX_SUPPORT_STREAM_NUMBER)
        {
            LOG_WARN(g_logger, "stream number exceed the max support number(5) in one AntPair!");
            break;
        }
        token = strtok(NULL, ",");
    }

    if ((stream_counter != 1) && (src_type == "rtmp"))
    {
        LOG_ERROR(g_logger, "_start_an_ant parameter wrong!!! do NOT support RTMP multi streams in one AntPair.");
        return -1;
    }

    vector<StreamId_Ext> stream_id_vec;
    for (int i = 0; i < stream_counter; i++)
    {
        StreamId_Ext id;
        stream_id_vec.push_back(id);
    }
    
    int j = 0;
    vector<string>::iterator it;
    for(it = stream_id_string_array.begin();
        it != stream_id_string_array.end();
        it++)
    {
        LOG_INFO(g_logger, "stream_id_string_array (*it):" << (*it));

        StreamId_Ext id;
        if (id.parse((*it)) < 0)
        {
            LOG_WARN(g_logger, "streamid invalid, id:" << (*it));
            return 400;
        }
        stream_id_vec[j] = id;
        if (_pair_map.find(stream_id_vec[j]) != _pair_map.end() && !restart)
        {
            LOG_WARN(g_logger, "stream: " << stream_id_vec[j].unparse() << " has already be kicked off.");
            return 409;
        }
        j++;
    }

    if (_current_ant_count >= _max_ant_count)
    {
        LOG_WARN(g_logger, "reach the up limit of number of ants");
        return 420;
    }

    //create n senders
    BufferQueue *queue = new BufferQueue[stream_counter];

    if (queue == NULL)
    {
        LOG_ERROR(g_logger, "BufferQueue array create failed!");
    }
    FLVSender *sender = new FLVSender[stream_counter];
    if (sender == NULL)
    {
        LOG_ERROR(g_logger, "FLVSender array create failed!");
    }

    int k = 0;
    for(it = stream_id_string_array.begin();
        it != stream_id_string_array.end();
        it++)
    {
        bool init_succ = true;
        queue[k].init(20, 50 * 1024);
        sender[k].init(dst_ip, dst_port, stream_id_vec[k], this);
        if (sender[k].init2() != 0)
        {
            LOG_WARN(g_logger, "FLVSender init failed.");
            init_succ &= false;
        }
        sender[k].set_event_base(_event_base);
        sender[k].set_buffer(queue+k);

        if ((*it).length() == 36 || (*it).length() == 32)
        {
            sender[k].id_version = 2;
        }
        else
        {
            sender[k].id_version = 1;
        }

        if (!init_succ)
        {
            if (sender != NULL)
            {
                delete[] sender;
                sender = NULL;
            }
            if (queue != NULL)
            {
                delete[] queue;
                queue = NULL;
            }
            return 503;
        }
        k++;
    }

    //4: link 1 ant, n buffers, and n senders into one AntPair.
    AntPair* pair;
    if (_pair_map.find(stream_id_vec[0]) != _pair_map.end()) //stream_id already exist, restarting
    {
        pair = _pair_map[stream_id_vec[0]];
        pair->status = ANT_PAIR_RESTARTING;
        pair->shutdown_flag = 0;
    }
    else
    {
        pair = new AntPair();
        pair->status = ANT_PAIR_STARTING;
        for(int i = 0; i < stream_counter; i++)
        {
            _pair_map[stream_id_vec[i]] = pair;
        }
    }

    pair->_stream_id_vec = stream_id_vec;
    pair->ant = NULL;   // will be assigned when sender started
    pair->rtmp_uri = URI::uri_decode(src_uri);

    if (pair->sender != NULL) delete[] pair->sender;
    pair->sender = sender;

    if (pair->buffer != NULL) delete[] pair->buffer;
    pair->buffer = queue;

    pair->type_string = src_type;
    pair->req_path = req_path;
    pair->stream_n_number = stream_counter;
    pair->stream_id_remove_flag = new int[pair->stream_n_number];

    for(int i = 0; i < pair->stream_n_number; i++)
    {
        pair->stream_id_remove_flag[i] = 0;
    }
    gettimeofday(&pair->last_start_time, 0);

    if (0 != plan_duration)
    {
        pair->plan_end_flag = true;
        pair->plan_end_time.tv_sec = pair->last_start_time.tv_sec + plan_duration;
    }

    //5, start ant, start n sender

    for(int i = 0; i < pair->stream_n_number; i++)
    {
        int status = sender[i].start();
        LOG_INFO(g_logger, "start a sender: " << pair->rtmp_uri << ". streamid: " << stream_id_vec[i].unparse().c_str());
        if (status != 0)
        {
            LOG_ERROR(g_logger, "sender start failed.");       
            return -1;
        }
    }

    return 0;
}

int AdminServer::_stop_an_ant(StreamId_Ext& stream_id)
{
    LOG_INFO(g_logger, "stop an ant. stream_id: " << stream_id.unparse() );
    map<StreamId_Ext, AntPair*>::iterator i = _pair_map.find(stream_id);
    if (i != _pair_map.end())
    {
        AntPair* pair = i->second;
        pair->stop(false);
        pair->status = ANT_PAIR_STOPED;
    }

    return 0;
}

void AdminServer::accept_handler(int fd, const short event)
{
    LOG_INFO(g_logger, "accept_handler: " << fd);
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int new_fd = accept(_listen_sock, (struct sockaddr *)&addr, &len);
    if (new_fd <= 0)
    {
        return;
    }
    if (net_util_set_nonblock(new_fd, 1) < 0)
    {
        return;
    }
    HTTPConnection *http = new HTTPConnection(new_fd, this, this, this);
    http->set_event_base(_event_base);
    _connect_map.insert(std::make_pair<int, HTTPConnection*>(new_fd, http));
    http->start();
}

int AdminServer::get(const char *req_path, Buffer *result_buf)
{
    // /start?src=rtmp://1.2.3.4:1935/app/4321&src_type=rtmp&dst_sid=23454&dst_host=2.3.4.5&dst_port=8888&dst_format=flv
    // /start?src=mmsh:10.10.1.1/&src_type=mms&dst_sid=23454&dst_host=2.3.4.5&dst_port=8888&dst_format=flv
    // /stop?dst_sid=23454

    int result = 200;
    string rsp_status = "200 OK";
    string response_content;

    LOG_INFO(g_logger, "ACCESS: " << URI::uri_decode(req_path));
    string req_path_string(req_path);
    URI uri;
    uri.parse_path(req_path);

    if (uri.get_file() == METHOD_START)
    {
        result = _http_start(req_path, response_content);
    }
    else if (uri.get_file() == METHOD_STOP)
    {
        result = _http_stop(req_path, response_content);
    }
    else if (uri.get_file() == METHOD_STATUS)
    {
        result = _http_status(req_path, response_content);
    }
    else if (uri.get_file() == METHOD_TEST)
    {
        result = _http_test(req_path, response_content);
    }
    else
    {
        result = 405;
        rsp_status = HttpServer::status_str(result);
        LOG_ERROR(g_logger, "result: " << rsp_status);
    }

    rsp_status = HttpServer::status_str(result);

    // compose response
    char* temp_buf = new char[1024];
    snprintf(temp_buf, 1024, "HTTP/1.0 %s\r\nServer: XMAnt Transformer\r\n"
        "Content-Type: text/plain\r\nContent-Length: %lu\r\n\r\n", rsp_status.c_str(), response_content.length());
    string response(temp_buf);
    delete[] temp_buf;
    response += response_content;
    size_t ret = result_buf->append_ptr(response.c_str(), response.length());
    if (ret != response.length())
    {
        LOG_FATAL(g_logger, "a FATAL error. need to be checked.");
        // return -1;
    }
    return 0;
}

int AdminServer::_http_start(string req_path, std::string& response)
{
    URI uri;
    uri.parse_path(req_path);

    int result = 200;
    string response_status;

    string src_type = uri.get_query(string("src_type"));
    if (src_type == "rtmp" || src_type == "rtmpt" || src_type == "mms" || src_type == "mmsh" || src_type == "mmst" || src_type == "flv")
    {
        int result_code = _start_an_ant(req_path, false);
        if (0 != result_code)
        {
            result = result_code;
            response_status = HttpServer::status_str(result);
            LOG_ERROR(g_logger, "result: " << response_status);
            response = "FAIL!!! \n";
        }
    }
    else
    {
        result = 415;
        response_status = HttpServer::status_str(result);
        LOG_ERROR(g_logger, "result: " << response_status << " type: " << uri.get_query(string("src_type")));
        response = "FAIL!!!  Unsupported src_type\n";
    }

    if (200 == result)
    {
        LOG_INFO(g_logger, "_http_start result: " << response_status);
        response = "SUCCESS!!!\n";
    }

    return result;
}

int AdminServer::_http_stop(std::string req_path, std::string& response)
{
    URI uri;
    uri.parse_path(req_path);
    int result = 200;
    string response_status;
    //char* token = NULL;    
    StreamId_Ext stream_id;

    //send the first stream id in the stop req_path, it will stop all n streams in one AntPair.
    string dst_sid_str = uri.get_query("dst_sid");

    if (dst_sid_str == "")
    {
        LOG_WARN(g_logger, "_http_stop parameter wrong!!! ");
        response = "FAIL!!! parameter wrong: no streamid\n";
        return -1;
    }
    assert(dst_sid_str != "");
    char dst_sid[256];
    memset(dst_sid, 0, sizeof(dst_sid));
    strncpy(dst_sid, dst_sid_str.c_str(), dst_sid_str.length());

    //token = strtok(dst_sid_str.c_str(),",");
    char *token = strtok(dst_sid, ",");
    string uuid_str = token;
    LOG_INFO(g_logger, "_http_stop, uuid_str:" << uuid_str);
        
    if (stream_id.parse(uuid_str) < 0)
    {
        LOG_WARN(g_logger, "streamid invalid, id:" << uuid_str);
        response = "FAIL!!! parameter wrong: streamid invalid\n";
        return -1;
    }

    // remove stream from scheduled_ant_map. so this ant will not start automatically.
    if (_pair_map.find(stream_id) != _pair_map.end())
    {
        if (0 != _stop_an_ant(stream_id))
        {
            result = 500;
            response_status = HttpServer::status_str(result);
            LOG_ERROR(g_logger, "result: " << response_status);
            response = "FAIL!!! Internal Server Error\n";
        }
        else
        {
            result = 200;
            response = "SUCCESS!!!\n";
        }
    }
    else
    {
        result = 404;
        response_status = HttpServer::status_str(result);
        LOG_ERROR(g_logger, "result: " << response_status);
        response = "FAIL!!! parameter wrong: Not found this streamid\n";
    }

    return result;
}

int AdminServer::_http_status(std::string req_path, std::string& response)
{
    URI uri;
    uri.parse_path(req_path);
    int result = 200;
    string rsp_status = "200 OK";

    string property = uri.get_query("property");
    if (property != "")
    {
        response = this->_build_status_info(property);
        return result;
    }
    
    response = this->_build_status_info("");
    return result;
}

int AdminServer::_http_test(std::string req_path, std::string& response)
{
    URI uri;
    uri.parse_path(req_path);
    int result = 200;
    string rsp_status = "200 OK";

    string crash = uri.get_query("crash");
    int c = atoi(crash.c_str());

    result = 1 / (c-1);
    result = 200;
    response = this->_build_status_info("");
    return result;

}

string AntPairStatusMsg[] =
{
    "ANT_PAIR_ACCEPT",
    "ANT_PAIR_STARTING",
    "ANT_PAIR_RUNNING",
    "ANT_PAIR_RESCHEDULED",
    "ANT_PAIR_RESTARTING",
    "ANT_PAIR_STOPED"
};

string AdminServer::_build_status_info(string property)
{
    Json::Value root;
    Json::FastWriter writer;
    
    if (property == "admin")
    {
        struct tm* p = localtime(&process_start_time);
        char ss[200];
        sprintf(ss, "%4.4d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
        root["process_start_time"] = string(ss);
    }
    
    if (property == "")
    {
        map<StreamId_Ext, AntPair*>::iterator it;
        int i = 0;
        for (it = this->_pair_map.begin();
            it != this->_pair_map.end();
            it++)
        {
            Json::Value node;
            node["index"] = i;
            AntPair* ant_pair = it->second;

            if (1 == ant_pair->stream_n_number)
            {
                StreamId_Ext id = ant_pair->_stream_id_vec[0];
                node["stream_id"] = id.unparse();
            }
            else
            {
                string id_array;

                for(int j = 0; j < ant_pair->stream_n_number; j++)
                {
                    StreamId_Ext id = ant_pair->_stream_id_vec[j];

                    if (j == (ant_pair->stream_n_number-1))
                    {
                        id_array = id_array + id.unparse();
                    }
                    else
                    {
                        id_array = id_array + id.unparse() + ", ";
                    }
                }
                node["stream_id"] = id_array;
            }

            unsigned int pos = ant_pair->req_path.find(' ');
            if (pos >= ant_pair->req_path.length())
            {
                pos = ant_pair->req_path.length();
            }

            node["req_path"] = ant_pair->req_path.substr(0, pos);
            node["continuous_failure_count"] = ant_pair->continuous_failure_count;

            struct timeval now_time;
            gettimeofday(&now_time, 0);

            node["latest_start_duration"] = (int)(now_time.tv_sec - it->second->last_start_time.tv_sec);
            node["ant_status"] = AntPairStatusMsg[ant_pair->status];
            node["type"] = it->second->type_string;

            i++;

            root.append(node);
        }
    }
    string json_str = root.toStyledString();
    return json_str;
}

int AdminServer::post(const char *req_path, const char *req_body, Buffer *result)
{
    // TODO
    return 0;
}

void AdminServer::http_finish_callback(int fd)
{
    /*
    map<int, HTTPConnection*>::iterator i = _connect_map.find(fd);
    if (i != _connect_map.end())
    {
        delete i->second;
        _connect_map.erase(i);
    }
    */
}

void AdminServer::ant_callback(const StreamId_Ext& streamid, AntEvent event, bool restart)
{
    AntPair *pair = NULL;
    map<StreamId_Ext, AntPair*>::iterator i = _pair_map.find(streamid);
    if (i != _pair_map.end())
    {
        pair = i->second;
    }
    else
    {
        assert(0);
        return;
    }

    if (event == ANT_EVENT_STOPED)
    {
        pair->shutdown_flag |= AntPair::FLAG_ANT_SHUTDOWN;
    }

    if (!restart)
    {
        pair->reschedule_flag = false;
    }
}

void AdminServer::sender_callback(const StreamId_Ext& streamid, SenderEvent event, bool restart)
{
    AntPair *pair = NULL;
    map<StreamId_Ext, AntPair*>::iterator i = _pair_map.find(streamid);
    if (i != _pair_map.end())
    {
        pair = i->second;
    }
    else
    {
        assert(0);
        return;
    }

    if (event == SENDER_EVENT_STARTED)
    {
        //one AntPair contain 1 ant and n senders, each of sender will call this sender_callback() function, 
        //but only allow ant start once for these n senders.
        if (!pair->ant_start_flag)
        {
            assert(pair->ant == NULL);
            if(!pair->ant)
            {
                if(pair->type_string == "rtmp")
                {
                    Ant *ant = new RTMPAnt(pair->_stream_id_vec, this);
                    ((RTMPAnt*)ant)->set_rtmp_uri(pair->rtmp_uri);
                    ant->set_buffer(pair->buffer);
                    pair->ant = ant;
                }
                else if (pair->type_string == "mms" || pair->type_string == "mmsh" || pair->type_string == "mmst" || pair->type_string == "flv")
                {
                    MMS2FLVAnt *mms2flvAnt = new MMS2FLVAnt(pair->_stream_id_vec, pair->stream_n_number, this);
                    mms2flvAnt ->set_config(pair->req_path,pair->stream_n_number);
                    mms2flvAnt ->set_buffer(pair->buffer);
                    pair->ant = mms2flvAnt;
                }
            }
            pair->ant->start();
            pair->ant_start_flag = true;
        }
    }
    else if (event == SENDER_EVENT_STOPED)
    {
        pair->shutdown_flag |= AntPair::FLAG_SENDER_SHUTDOWN;
    }

    if (!restart)
    {
        pair->reschedule_flag = false;
    }
}

void AdminServer::_start_timer(uint32_t sec, uint32_t usec)
{
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = usec;

    levtimer_set(&_timer_event, timer_event_handler, this);
    event_base_set(_event_base, &_timer_event);
    levtimer_add(&_timer_event, &tv);
}

void AdminServer::on_timer(const int fd, const short event)
{
    // LOG_DEBUG(g_logger, "AdminServer::on_timer(). fd= " << fd);
    map<StreamId_Ext, AntPair*>::iterator i = _pair_map.begin();

    for (; i != _pair_map.end(); i++)
    {
        AntPair* pair = i->second;
        StreamId_Ext stream_id = pair->_stream_id_vec[0];
        struct timeval now_time;
        gettimeofday(&now_time, 0);

        switch (pair->status)
        {
        LOG_INFO(g_logger, "AdminServer:: on_timer, pair->status= " << pair->status);
        case ANT_PAIR_ACCEPT:
        case ANT_PAIR_STARTING:
        case ANT_PAIR_RESTARTING:
        {
            switch (pair->check_restart())
            {
                case AntPair::RESTART_STRATEGY_SUCCESS:
                    pair->status = ANT_PAIR_RUNNING;
                    break;

                case AntPair::RESTART_STRATEGY_WAIT:
                    break;

                case AntPair::RESTART_STRATEGY_RESTART:
                    // if an ant exit without stop signal, we should restart it.
                    LOG_INFO(g_logger, "AdminServer:: try to restart stream, stream id: " << pair->_stream_id_vec[0].unparse());
                    pair->stop(true);
                    pair->status = ANT_PAIR_RUNNING;
                    break;

                case AntPair::RESTART_STRATEGY_STOP:
                    pair->stop(false);
                    pair->status = ANT_PAIR_RUNNING;
                    break;

                default:
                    break;
            }
        }
        break;

        case ANT_PAIR_RUNNING:
        {
            if ((pair->shutdown_flag & AntPair::FLAG_ANT_SHUTDOWN)
                 == AntPair::FLAG_ANT_SHUTDOWN)
            {
                 if ((pair->ant) && (!pair->ant->is_active()))
                 {
                     LOG_DEBUG(g_logger, "AdminServer::on_timer delete ant");
                     delete pair->ant;
                     pair->ant = NULL;
                 }

                 if (pair->sender)
                 {
                     for(int i = 0; i < pair->stream_n_number; i++)
                     {
                         LOG_DEBUG(g_logger, "AdminServer::on_timer stop sender[i]:" << i);
                         pair->sender[i].stop();
                     }
                 }
            }

            if ((pair->shutdown_flag & AntPair::FLAG_SENDER_SHUTDOWN)
                 == AntPair::FLAG_SENDER_SHUTDOWN)
            {
                 if (pair->sender)
                 {
                     LOG_DEBUG(g_logger, "AdminServer::on_timer delete sender");
                     delete[] pair->sender;
                     pair->sender = NULL;
                 }
                 if (pair->ant)
                 {
                     pair->ant->stop();
                 }
            }

            if (pair->plan_end_flag && (now_time.tv_sec == pair->plan_end_time.tv_sec))
            {
                LOG_DEBUG(g_logger, "AdminServer::on_timer plan_end_time here!!!");
                if (0 != _stop_an_ant(stream_id))
                {
                    LOG_ERROR(g_logger, "plan_end_time fail!!! ");
                    // hechao fixed
                    //return -1;
                    return;
                }
            }

            if (!pair->sender && !pair->ant)
            {
                if (pair->reschedule_flag)
                {
                    pair->status = ANT_PAIR_RESCHEDULED;
                }
                else
                {
                    pair->status = ANT_PAIR_STOPED;
                }
            }
        }
        break;

        case ANT_PAIR_RESCHEDULED:
        {   
            if (pair->sender || pair->ant)
            {
                LOG_ERROR(g_logger, "pair does not stop normally, stream id: " << pair->_stream_id_vec[0].unparse());
                pair->stop(false);
                break;
            }

            switch (pair->check_reschedule())
            {
                case AntPair::RESTART_STRATEGY_STOP:
                    LOG_WARN(g_logger, "AdminServer:: Ant Stopped by continuous failure, stream id: " << pair->_stream_id_vec[0].unparse());
                    pair->status = ANT_PAIR_STOPED;
                    break;

                case AntPair::RESTART_STRATEGY_WAIT:
                    break;

                case AntPair::RESTART_STRATEGY_RESTART:
                    // if an ant exit without stop signal, we should restart it.
                    LOG_INFO(g_logger, "AdminServer:: try to restart stream, stream id: " << pair->_stream_id_vec[0].unparse());

                    _start_an_ant(pair->req_path, true);
                    pair->status = ANT_PAIR_RESTARTING;
                    break;

                default:
                    break;
            }
        }
        break;

        case ANT_PAIR_STOPED:
        {
            if (!pair->ant && !pair->sender)
            {
                if (pair->buffer)
                {
                    delete[] pair->buffer;
                    pair->buffer = NULL;
                }

                //when delete one stream in _pair_map, will also delete other (n-1) streams in the same AntPair.
                map<StreamId_Ext, AntPair*>::iterator j = i--;
                int count = 0;

                for(int k = 0; k < pair->stream_n_number; k++)
                {
                    if (j->first == j->second->_stream_id_vec[k])
                    {
                        j->second->stream_id_remove_flag[k] = 1;
                    }
                }

                for (int k = 0; k < pair->stream_n_number; k++)
                {
                    if (j->second->stream_id_remove_flag[k])
                    {
                        count++;
                    }
                }

                if (count == pair->stream_n_number)
                {
                    //only all n streams in one AntPair be removed from _pair_map, then delete the AntPair.
                    delete j->second;
                    j->second = NULL;
                }
                _pair_map.erase(j);

            }
            else
            {
                LOG_INFO(g_logger, "waiting for ant and sender stop. stream_id: " << stream_id.unparse());
                pair->status = ANT_PAIR_RUNNING;
            }
        }
        break;

        default:
            break;
        }

        _current_ant_count = _pair_map.size();
    }

    // if any http connection stopped, delete http_connect.
    map<int, HTTPConnection*>::iterator connect_map_it;
    for (connect_map_it = _connect_map.begin();
        connect_map_it != _connect_map.end();
        connect_map_it++)
    {
        if (connect_map_it->second->stopped)
        {
            map<int, HTTPConnection*>::iterator temp_it = connect_map_it;
            connect_map_it--;
            delete temp_it->second;
            temp_it->second = NULL;
            _connect_map.erase(temp_it);
        }
    }

    if (this->_force_exit_flag)
    {
        this->force_exit_all();
    }

    _start_timer(0, 10000);
}

int AdminServer::force_exit_all()
{
    LOG_INFO(g_logger, "AdminServer: force_exit_all");

    this->_force_exit_flag = true;

    if (_pair_map.size() == 0)
    {
        exit(0);
    }

    map<StreamId_Ext, AntPair*>::iterator pair_map_it;
    for (pair_map_it = this->_pair_map.begin();
        pair_map_it != this->_pair_map.end();
        pair_map_it++)
    {
        if (pair_map_it->second != NULL)
        {
            if (pair_map_it->second->ant != NULL)
            {
                pair_map_it->second->ant->force_exit();
            }
            if (pair_map_it->second->sender != NULL)
            {
                pair_map_it->second->sender->force_exit();
            }
        }
    }

    return 0;
}
