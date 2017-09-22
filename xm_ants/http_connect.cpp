/*
 *
 *
 */

#include "http_connect.h"
#include <assert.h>
#include "admin_server.h"

using namespace lcdn;
using namespace lcdn::base;

extern "C" void _http_handler(int fd, const short event, void *arg);

void HTTPConnection::timer_event_handler(const int fd, const short event, void *ctx)
{
    static_cast<HTTPConnection *>(ctx)->on_timer(fd, event);
}

HTTPConnection::HTTPConnection(int fd,
    Getable *getter, Postable *poster, AdminServer * admin,
    size_t buf_size)
    : stopped(false), _fd(fd), _buf_size(buf_size), _read_buf(NULL), _write_buf(NULL),
    _is_ev_monitored(false), _timer_enable(false),
    _getter(getter), _poster(poster), _admin(admin)
{
    _read_buf = new Buffer(_buf_size);
    _write_buf = new Buffer(_buf_size);
    _state = CTX_STATE_INIT;
    this->_last_active_time = time(NULL);
}

HTTPConnection::~HTTPConnection()
{
    // make sure event was deleted. 
    this->_stop();

    if (_read_buf)
    {
        delete _read_buf;
        _read_buf = NULL;
    }
    if (_write_buf)
    {
        delete _write_buf;
        _write_buf = NULL;
    }
}

int HTTPConnection::init()
{
    //TODO 
    return 0;
}

int HTTPConnection::start()
{
    levent_set(&_event, _fd, EV_READ | EV_PERSIST, _http_handler, this);
    event_base_set(_event_base, &_event);
    levent_add(&_event, 0);
    _is_ev_monitored = true;

    _last_active_time = time(NULL);
    this->_start_timer();

    return 0;
}

void HTTPConnection::http_handler(int fd, const short event)
{
    if (event & EV_READ)
    {
        http_read_handler(fd, event);
    }
    else if (event & EV_WRITE)
    {
        http_write_handler(fd, event);
    }
    else
    {
        LOG_ERROR(g_logger, "event invalid, event: " << event);
    }

    if (_state == CTX_STATE_END)
    {
        this->_stop();
    }

    _last_active_time = time(NULL);
}

void HTTPConnection::http_read_handler(int fd, const short event)
{
    if (_read_buf->read_fd(fd) < 0)
    {
        // TODO handle read error
        _state = CTX_STATE_END;
    }

    switch(_state)
    {
        case CTX_STATE_INIT:
        case CTX_STATE_READING:
            {
                int ret = _parse_http_req(_read_buf);
                if (ret < 0)
                {
                    _state = CTX_STATE_END;
                    break;
                }
                _state = CTX_STATE_SENDING;
                _enable_write();
                break;
            }
        default:
            break;
    }
}

void HTTPConnection::http_write_handler(int fd, const short event)
{
    if (_write_buf->write_fd(fd) < 0)
    {
        // TODO handle read error
        _state = CTX_STATE_END;
        return;
    }
    switch(_state)
    {
        case CTX_STATE_SENDING:
            if (_write_buf->data_len() == 0)
            {
                _disable_write();
                _state = CTX_STATE_END;
            }
            break;
        default:
            break;
    }
}

void HTTPConnection::_finish()
{
    // TODO
    // call back
    _admin->http_finish_callback(_fd);
}

void HTTPConnection::_stop()
{
    // TODO
    if (_is_ev_monitored)
    {
        levent_del(&_event);
        _is_ev_monitored = false;
    }

    if (this->_fd > 0)
    {
        if (close(this->_fd) >= 0)
        {
            this->_fd = -1;
        }
    }

    this->_stop_timer();

    this->stopped = true;
}

void HTTPConnection::on_timer(const int fd, const short event)
{
    this->_timer_enable = false;

    if (this->_idle_time() > 15)
    {
        this->_stop();
        return;
    }

    this->_start_timer();
}

void HTTPConnection::_start_timer()
{
    // every 5 seconds check status.
    this->_start_timer(5, 0);
}

void HTTPConnection::_start_timer(uint32_t sec, uint32_t usec)
{
    LOG_DEBUG(g_logger, "HTTPConnection::_start_timer");
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = usec;

    levtimer_set( &(this->_timer_event), HTTPConnection::timer_event_handler, this);
    event_base_set(this->_event_base, &(this->_timer_event));
    levtimer_add( &(this->_timer_event), &tv);
    this->_timer_enable = true;
}

void HTTPConnection::_stop_timer()
{
    LOG_DEBUG(g_logger, "HTTPConnection::_stop_timer");
    if (this->_timer_enable)
    {
        levtimer_del( &(this->_timer_event));
        this->_timer_enable = false;
    }
}

int HTTPConnection::_idle_time()
{
    return time(NULL) - this->_last_active_time;
}

// @return:
//      -1: error request
//      0:  succ
//      1:  need more data
int HTTPConnection::_parse_http_req(Buffer * buf)
{
    if (buf->data_len() > 2*1024)
    {
        // huge request is unsupported
        return -1;
    }
    std::string temp_str((const char *)(buf->data_ptr()),buf->data_len());
    if (temp_str.find("\r\n\r\n"))
    {
        std::string first_line = temp_str.substr(0, temp_str.find("\r\n"));
        if (first_line.find("GET") == 0)
        {
            return _getter->get(temp_str.substr(4).c_str(), _write_buf);
        }
        else
        {
            // TODO
            // 503 Service Unavailable
            return -1;
        }

    }
    else
    {
        return 1;
    }

    return 0;
}

void HTTPConnection::_enable_write()
{
    assert (_is_ev_monitored == true);
    levent_del(&_event);
    levent_set(&_event, _fd, EV_READ | EV_WRITE | EV_PERSIST,
            _http_handler, this);
    event_base_set(_event_base, &_event);
    levent_add(&_event, 0);
}

void HTTPConnection::_disable_write()
{
    assert (_is_ev_monitored == true);
    levent_del(&_event);
    levent_set(&_event, _fd, EV_READ | EV_PERSIST,
            _http_handler, this);
    event_base_set(_event_base, &_event);
    levent_add(&_event, 0);
}

extern "C" void _http_handler(int fd, const short event, void *arg)
{
    static_cast<HTTPConnection *>(arg)->http_handler(fd, event);
}

