/**
 * @file http_server.cpp
 * @brief   a simple http server.
 * @author songshenyi
 * <pre><b>copyright: Youku</b></pre>
 * <pre><b>email: </b>songshenyi@youku.com</pre>
 * <pre><b>company: </b>http://www.youku.com</pre>
 * <pre><b>All rights reserved.</b></pre>
 * @date 2014/08/13
 * @see http_server.h
 */

#include <stdio.h>
#include "http_server.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "logger.h"

using namespace std;

SessionManager::SessionManager(int session_timeout_in_sec)
: _session_timeout_in_sec(session_timeout_in_sec)
{

}

void SessionManager::attach(Session* session, uint64_t current_tick)
{
    if (session == NULL)
    {
        return;
    }
    session->last_active = current_tick;
    _session_store.push_back(session);
    session->it = _session_store.end();
    session->it--;
}

void SessionManager::detach(Session* session)
{
    if (session == NULL)
    {
        return;
    }

    _session_store.erase(session->it);
}

void SessionManager::update_duration(Session* session, uint64_t current_tick, uint64_t new_duration)
{
    session->last_active = current_tick;
    session->duration = new_duration;
}

void SessionManager::on_active(Session*session, uint64_t current_tick)
{
    session->last_active = current_tick;
}

SessionManager::~SessionManager()
{
    _session_store.clear();
}


int HttpServerConfig::load(Json::Value& root)
{

}


int SocketUtil::set_nonblock(int fd, bool on)
{
    int flags = 1;

    if (fd > 0) 
    {
        flags = fcntl(fd, F_GETFL);
        if (-1 == flags)
        {
            return -1;
        }
            
        if (on)
        {
            flags |= O_NONBLOCK;
        }
        else
        {
            flags &= ~O_NONBLOCK;
        }

        if (0 != fcntl(fd, F_SETFL, flags))
        {
            return -2;
        }
    }
    return 0;
}

int SocketUtil::set_cloexec(int fd, bool on)
{
    fcntl(fd, F_SETFD, on ? FD_CLOEXEC : 0);
}

static void http_accept(const int fd, const short which, void *arg)
{
    HttpServer* server = (HttpServer*)arg;
    server->accept(fd, which, arg);
}

HttpServer::HttpServer()
{

}

int HttpServer::init(event_base* main_base, Json::Value& config)
{
    _main_base = main_base;
    if (main_base == NULL)
    {
        LOG_ERROR(g_logger, "main_base is NULL.");
        return -1;
    }

    _config = new HttpServerConfig();
    if (_config->load(config) != 0)
    {
        LOG_ERROR(g_logger, "http server config load failed.");
        return -1;
    }
    _session_manager = new SessionManager(_config->timeout_second);

    _listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (_listen_fd == -1)
    {
        LOG_ERROR(g_logger, "socket failed. error =" << strerror(errno));
        return -1;
    }

    int ret = set_sockopt(_listen_fd);

    if (ret != 0)
    {
        return -1;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_aton("0.0.0.0", &(addr.sin_addr));
    addr.sin_port = htons(_config->listen_port);

    ret = ::bind(_listen_fd, (sockaddr*)&addr, sizeof(addr));
    if (ret != 0)
    {
        LOG_ERROR(g_logger, "bind addr failed. error =" << strerror(errno));
        return -1;
    }

    ret = listen(_listen_fd, 128);
    if (ret != 0)
    {
        LOG_ERROR(g_logger, "listen http fd failed. error =" << strerror(errno));
        return -1;
    }

    levent_set(&_ev_listener, _listen_fd, EV_READ | EV_PERSIST, http_accept, (void*)this);
    event_base_set(_main_base, &_ev_listener);
    levent_add(&_ev_listener, 0);
}

int HttpServer::set_sockopt(int fd)
{
    int val = 1;
    char error_str[1024];

    if (-1 == setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&val, sizeof(val)))
    {
        LOG_ERROR(g_logger, "set SO_REUSEADDR failed. error =" << strerror(errno));
        return -1;
    }

    SocketUtil::set_cloexec(fd, true);

    int ret = SocketUtil::set_nonblock(fd, true);
    if (ret != 0)
    {
        sprintf(error_str, "set nonblock failed. ret = %d, error = %s", ret, strerror(errno));
        LOG_ERROR(g_logger, string(error_str));
        return -1;
    }

    val = 1;
    ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&val, sizeof(val));
    if (ret != 0)
    {
        sprintf(error_str, "set tcp nodelay failed. error = %s", strerror(errno));
        LOG_ERROR(g_logger, string(error_str));
        return -1;
    }

#ifdef TCP_DEFER_ACCEPT
    int timeout = 30; /* 30 seconds */
    ret = setsockopt(fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &timeout, sizeof(timeout));
    if (ret != 0)
    {
        sprintf(error_str, "set tcp nodelay failed. fd = %d error = %s", _listen_fd, strerror(errno));
        LOG_WARN(g_logger, error_str);
    }
#endif
    
    return 0;
}

void HttpServer::accept(const int fd, const short which, void* arg)
{

}

void HttpServer::finalize()
{

}

string HttpServer::status_str(int code)
{
    string str;
    switch(code)
    {
    case 100:
        str = "100 Continue";
        break;

    case 101:
        str = "101 Switching Protocols";
        break;

    case 102:
        str = "102 Processing";
        break;

    case 200:
        str = "200 OK";
        break;

    case 201:
        str = "201 Created";
        break;

    case 202:
        str = "202 Accepted";
        break;

    case 203:
        str = "203 Non-Authoritative Information";
        break;

    case 204:
        str = "204 No Content";
        break;

    case 205:
        str = "205 Reset Content";
        break;

    case 206:
        str = "206 Partial Content";
        break;

    case 207:
        str = "207 Multi-Status";
        break;

    case 208:
        str = "208 Already Reported";
        break;

    case 300:
        str = "300 Multiple Choices";
        break;

    case 301:
        str = "301 Moved Permanently";
        break;

    case 302:
        str = "302 Found";
        break;

    case 303:
        str = "303 See Other";
        break;

    case 304:
        str = "304 Not Modified";
        break;

    case 305:
        str = "305 Use Proxy";
        break;

    case 306:
        str = "306 Switch Proxy";
        break;

    case 307:
        str = "307 Temporary Redirect";
        break;

    case 308:
        str = "308 Permanent Redirect";
        break;

    case 400:
        str = "400 Bad Request";
        break;

    case 401:
        str = "401 Unauthorized";
        break;

    case 402:
        str = "402 Payment Required";
        break;

    case 403:
        str = "403 Forbidden";
        break;

    case 404:
        str = "404 Not Found";
        break;

    case 405:
        str = "405 Method Not Allowed";
        break;

    case 406:
        str = "406 Not Acceptable";
        break;

    case 407:
        str = "407 Proxy Authentication Required";
        break;

    case 408:
        str = "408 Request Timeout";
        break;

    case 409:
        str = "409 Conflict";
        break;

    case 410:
        str = "410 Gone";
        break;

    case 411:
        str = "411 Length Required";
        break;

    case 412:
        str = "412 Precondition Failed";
        break;

    case 413:
        str = "413 Request Entity Too Large";
        break;

    case 414:
        str = "414 Request-URI Too Long";
        break;

    case 415:
        str = "415 Unsupported Media Type";
        break;

    case 416:
        str = "416 Requested Range Not Satisfiable";
        break;

    case 417:
        str = "417 Expectation Failed";
        break;

    case 418:
        str = "418 I'm a teapot";
        break;

    case 419:
        str = "419 Authentication Timeout";
        break;

    case 420:
        str = "420 Method Failure";
        break;

    case 422:
        str = "422 Unprocessable Entity";
        break;

    case 423:
        str = "423 Locked";
        break;

    case 424:
        str = "424 Failed Dependency";
        break;

    case 426:
        str = "426 Upgrade Required";
        break;

    case 428:
        str = "428 Precondition Required";
        break;

    case 429:
        str = "429 Too Many Requests";
        break;

    case 431:
        str = "431 Request Header Fields Too Large";
        break;

    case 440:
        str = "440 Login Timeout";
        break;

    case 444:
        str = "444 No Response";
        break;

    case 449:
        str = "449 Retry With";
        break;

    case 450:
        str = "450 Blocked by Windows Parental Controls";
        break;

    case 451:
        str = "451 Unavailable For Legal Reasons";
        break;

    case 494:
        str = "494 Request Header Too Large";
        break;

    case 495:
        str = "495 Cert Error";
        break;

    case 496:
        str = "496 No Cert";
        break;

    case 497:
        str = "497 HTTP to HTTPS";
        break;

    case 499:
        str = "499 Client Closed Request";
        break;

    case 500:
        str = "500 Internal Server Error";
        break;

    case 501:
        str = "501 Not Implemented";
        break;

    case 502:
        str = "502 Bad Gateway";
        break;

    case 503:
        str = "503 Service Unavailable";
        break;

    case 504:
        str = "504 Gateway Timeout";
        break;

    case 505:
        str = "505 HTTP Version Not Supported";
        break;

    case 506:
        str = "506 Variant Also Negotiates";
        break;

    case 507:
        str = "507 Insufficient Storage";
        break;

    case 508:
        str = "508 Loop Detected";
        break;

    case 509:
        str = "509 Bandwidth Limit Exceeded";
        break;

    case 510:
        str = "510 Not Extended";
        break;

    case 511:
        str = "511 Network Authentication Require";
        break;

    default:
        str = user_define_str(code);
        break;
    }
     
    return str;
}

string HttpServer::user_define_str(int code)
{
    string str;
    switch (code)
    {
    default:
        str = code + " Continue";
        break;
    }

    return str;
}
