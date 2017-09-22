/**
 * @file http_server.h
 * @brief   a simple http server.
 * @author songshenyi
 * <pre><b>copyright: Youku</b></pre>
 * <pre><b>email: </b>songshenyi@youku.com</pre>
 * <pre><b>company: </b>http://www.youku.com</pre>
 * <pre><b>All rights reserved.</b></pre>
 * @date 2014/08/13
 */

#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include <stdint.h>
#include <deque>
#include <list>
#include <string>
#include <tr1/functional>

#include <utils/buffer.hpp>
#include <json-c++/json.h>

#include "levent.h"
#include "global_var.h"

#ifdef _WIN32
#define __null 0
#endif
#define MAX_SESSION_TIMEOUT 60


class Session;

typedef std::tr1::function<void(Session *session)> session_handler;

class Session
{
public:
    uint64_t last_active;
    uint64_t duration;
    std::list<Session*>::iterator it;
    session_handler timeout_func;
};

class SessionManager
{
public:
    SessionManager(int session_timeout_in_sec = MAX_SESSION_TIMEOUT);

    void attach(Session* session, uint64_t current_tick);

    void detach(Session* session);

    void update_duration(Session* session, uint64_t current_tick, uint64_t new_duration);

    void on_active(Session*session, uint64_t current_tick);

    void heart_beat(uint64_t current_tick);

    ~SessionManager();

protected:
    std::list<Session*> _session_store;

    int _session_timeout_in_sec;
};

class HttpConection;

class HttpSession : public Session
{
public:
    HttpConection* connection;
};

typedef std::tr1::function<int(std::string path, std::string param, std::string& response)> http_handler;

class HttpConection
{
public:
    int fd;
    struct event ev;
    char remote_ip[32];
    uint16_t remote_port;
    lcdn::base::Buffer *read_buf;
    lcdn::base::Buffer *write_buf;
    HttpSession* session;
    bool flush;
    std::string path;
    std::string param;
    http_handler handler;
};

class SocketUtil
{
public:
    static int set_nonblock(int fd, bool on);
    static int set_cloexec(int fd, bool on);
};

class HttpServerConfig
{
public:
    int load(Json::Value& root);

    Json::Value get_config();

    std::string get_config_str();

public:
    uint16_t listen_port;
    int timeout_second;
    int max_connections;
    size_t buffer_max;

protected:
    Json::Value _dump_config;
    std::string _dump_config_str;
};

class HttpServer
{
public:
    HttpServer();

    int init(event_base* main_base, Json::Value& config);

    void accept(const int fd, const short which, void* arg);

    void connect_handler(const int fd, const short which, void* arg);

    void read_handler(HttpConection* connection);

    void write_handler(HttpConection* connection);

    int set_sockopt(int fd);

    void timeout_handler(Session* session);

    void close_connection(HttpConection* http_connection);

    int add_handler(std::string path, std::string param, http_handler handler);

    http_handler find_handler(std::string path);

    void log_access();

    void finalize(); 

    static std::string status_str(int code);

    static std::string user_define_str(int code);

    ~HttpServer();

protected:
    event_base* _main_base;

    SessionManager* _session_manager;

    int _listen_fd;

    HttpServerConfig* _config;

    struct event _ev_listener;
};

#endif /* HTTP_SERVER_H_ */
