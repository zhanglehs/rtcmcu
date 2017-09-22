/*
 * author: hechao@youku.com
 * create: 2014/3/10
 *
 */

#include "flv_sender.h"
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/net_utils.h>
#include <iostream>
#include "proto.h"
#include "global_var.h"
#include "admin_server.h"

using namespace lcdn;
using namespace lcdn::base;

#ifdef _WIN32
#define NULL 0
#endif

static void event_handler(const int fd, const short event, void *ctx)
{
    static_cast<FLVSender *>(ctx)->event_handler_impl(fd, event);
}

static void timer_event_handler(const int fd, const short event, void *ctx)
{
    static_cast<FLVSender *>(ctx)->on_timer(fd, event);
}

FLVSender::FLVSender()
:   _sock(-1),
    _event_base(NULL),
    _timer_enable(false),
    _event_enable(false),
    _send_buf(NULL),
    _receive_buf(NULL),
    _trigger_off(false),
    _force_exit(false)
{
}

int FLVSender::init(const std::string receiver_ip,
        uint16_t receiver_port, StreamId_Ext& stream_id,
        AdminServer *admin)
{
    _receiver_ip = receiver_ip;
    _receiver_port = receiver_port;
    _stream_id = stream_id;
    _admin = admin;
    _last_active_time = time(NULL);
    _total_sent_bytes = 0;
    return 0;
}

FLVSender::FLVSender(const std::string receiver_ip,
        uint16_t receiver_port, StreamId_Ext& stream_id,
        AdminServer *admin)
:_receiver_ip(receiver_ip),
    _receiver_port(receiver_port),
    _stream_id(stream_id),
    _sock(-1),
    _event_base(NULL),
    _timer_enable(false),
    _event_enable(false),
    _send_buf(NULL),
    _receive_buf(NULL),
    _admin(admin),
    _trigger_off(false),
    _force_exit(false)
{
    _last_active_time = time(NULL);
    _total_sent_bytes = 0;
}

FLVSender::~FLVSender()
{
    if (_send_buf)
    {
        delete _send_buf;
        _send_buf = NULL;
    }
    if (_receive_buf)
    {
        delete _receive_buf;
        _receive_buf = NULL;
    }

    if (_timer_enable)
    {
        levtimer_del(&_timer_event);
        _timer_enable = false;
    }

    if (this->_event_enable)
    {
        levent_del(&_event);
        this->_event_enable = false;
    }

#ifdef DUMP_SENDER
    fclose(sender_dump_fp);
#endif 
}

int FLVSender::init2()
{
    LOG_DEBUG(g_logger, "FLVSender::init");
    if (!_send_buf)
    {
        _send_buf = new Buffer(2*1024*1024);
    }
    if (!_receive_buf)
    {
        _receive_buf = new Buffer(2*1024*1024);
    }

    if (!_send_buf || !_receive_buf)
    {
        goto failed;
    }
    _state = FSStateInit;

#ifdef DUMP_SENDER
    string sender_dump_file_name = "sender_dump.flv";
    FILE* sender_dump_fp = fopen(sender_dump_file_name.c_str(), "wb");
    if (sender_dump_fp < 0)
    {
        LOG_WARN(g_logger, "FLVSender: open sender dump file failed. error: " << strerror(errno));
    }
#endif
    return 0;

failed:
    if (_send_buf)
    {
        delete _send_buf;
        _send_buf = NULL;
    }
    if (_receive_buf)
    {
        delete _receive_buf;
        _receive_buf = NULL;
    }
    return -1;

}

void FLVSender::set_event_base(struct event_base *base)
{
    LOG_DEBUG(g_logger, "FLVSender::set_event_base");
    _event_base = base;
}

int FLVSender::start()
{
    LOG_DEBUG(g_logger, "FLVSender::start");
    _sock = socket(AF_INET, SOCK_STREAM, 0);
    if (_sock < 0)
    {
        LOG_ERROR(g_logger, "create socket error. errno: " << errno);
        return -1;
    }
    if (net_util_set_nonblock(_sock, 1) < 0)
    {
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    if(0 == inet_aton(_receiver_ip.c_str(), &addr.sin_addr))
    {
        close(_sock);
        _sock = -1;
        LOG_ERROR(g_logger, "inet_aton failed. receiver_ip = " << _receiver_ip);
        return -1;
    }

    addr.sin_port = htons(_receiver_port);
    int r = connect(_sock, (struct sockaddr *) &addr, sizeof(addr));
    if (r == 0)
    {
        LOG_INFO(g_logger, "receiver connected success.");
        _state = FSStateConnected;
    }
    else
    {
        if (EINPROGRESS != errno
                && EALREADY != errno
                && EWOULDBLOCK != errno)
        {
            LOG_ERROR(g_logger, "FLVSender: connect failed. error = " << strerror(errno));
            close(_sock);
            _sock = -1;
            return -1;
        }
        LOG_INFO(g_logger, "FLVSender: receiver connecting...");
        _state = FSStateConnecting;
    }

    levent_set(&_event, _sock, EV_READ | EV_WRITE | EV_PERSIST, event_handler, this);
    event_base_set(_event_base, &_event);
    levent_add(&_event, 0);
    this->_event_enable = true;

    _start_timer();

    return 0;
}

void FLVSender::_enable_write()
{
    LOG_DEBUG(g_logger, "FLVSender::_enable_write");
    //if(FSStateConnected == _state)
    {
        levent_del(&_event);
        levent_set(&_event, _sock, EV_READ | EV_WRITE | EV_PERSIST,
                event_handler, this);
        event_base_set(_event_base, &_event);
        levent_add(&_event, 0);
    }
}

void FLVSender::_disable_write()
{
    LOG_DEBUG(g_logger, "FLVSender::_disable_write");
    //if(FSStateConnected == _state)
    {
        levent_del(&_event);
        levent_set(&_event, _sock, EV_READ | EV_PERSIST,
                event_handler, this);
        event_base_set(_event_base, &_event);
        levent_add(&_event, 0);
    }
}

void FLVSender::event_handler_impl(const int fd, const short event)
{
    if (this->_trigger_off)
    {
        this->_state = FSStateEnd;
    }

    if (_state == FSStateEnd)
    {
        LOG_DEBUG(g_logger, "_state == FSStateEnd");
        _stop(true);
        return;
    }

    if (id_version == 1)
    {
        event_handler_impl_v1(fd, event);
    }
    else if (id_version == 2)
    {
        event_handler_impl_v2(fd, event);
    }
    else
    {
        LOG_ERROR(g_logger, "id_version invalid, stream id:" << _stream_id.unparse());;
    }
}

void FLVSender::event_handler_impl_v1(const int fd, const short event)
{
    LOG_DEBUG(g_logger, "FLVSender::event_handler_impl_v1");
    if(this->_trigger_off)
    {
        this->_state = FSStateEnd;
    }

    if (event & EV_READ)
    {
        _read_handler_impl_v1(fd, event);
    }
    else if (event & EV_WRITE)
    {
        _write_handler_impl_v1(fd, event);
    }
    else
    {
        LOG_ERROR(g_logger, "event invalid, event: " << event);
    }
}

void FLVSender::_read_handler_impl_v1(const int fd, const short event)
{
    LOG_DEBUG(g_logger, "FLVSender::_read_handler_impl_v1");
    if (_receive_buf->read_fd(_sock) < 0)
    {
        LOG_WARN(g_logger, "FLVSender: _read_handler_impl: read from receiver error. stream_id: "<< _stream_id.unparse());
        _state = FSStateEnd;
        return;
    }

    _last_active_time = time(NULL);
    switch(_state)
    {
        case FSStateRspReceiving:
            {
                u2r_rsp_state rsp;
                int ret = decode_u2r_rsp_state(&rsp, _receive_buf);
                if (ret < 0)
                {
                    LOG_DEBUG(g_logger, "_read_handler_impl: decode_u2r_rsp_state error.");
                    _state = FSStateEnd;
                    return;
                }
                else if (ret == 0)
                {
                    if (rsp.result == 0)
                    {
                        _state = FSStateStartStreaming;
                        _enable_write();
                        if (_admin)
                        {
                            _admin->sender_callback(_stream_id, SENDER_EVENT_STARTED, true);
                        }
                    }
                    else
                    {
                        LOG_DEBUG(g_logger, "_read_handler_impl: u2r_rsp_state return error. result: " << rsp.result);
                        _state = FSStateEnd;
                        return;
                    }
                }
                else
                {
                    // need more read
                }
                break;
            }
        default:
            // TODO
            break;
    }
}

void FLVSender::_write_handler_impl_v1(const int fd, const short event)
{
    LOG_DEBUG(g_logger, "_write_handler_impl_v1");

    do
    {
        LOG_DEBUG(g_logger, "datasize: " << _send_buf->data_len());

        if (_send_buf->data_len() > 0)
        {
            int write_bytes = _send_buf->write_fd(_sock);
            if (write_bytes < 0)
            {
                // TODO 
                LOG_WARN(g_logger, "write to receiver error");
                _state = FSStateEnd;
                break;
            }
            else
            {
                _total_sent_bytes += write_bytes;
            }
        }

        if (_send_buf->data_len() > 0)
        {
            break;
        }

        _send_buf->try_adjust();

        switch(_state)
        {
            case FSStateConnecting:
                {
                    char t;

                    if (0 != read(_sock, &t, 0))
                    {
                        _state = FSStateEnd;
                        LOG_WARN(g_logger, "connect to receiver error");
                        return;
                    }

                    _state = FSStateConnected;
                    LOG_DEBUG(g_logger, "connected to receiver.");
                }

            case FSStateConnected:
                {
                    u2r_req_state req;
                    req.version = 0;
                    req.streamid = _stream_id.get_32bit_stream_id();
                    
                    memset((char*)req.token, 0, sizeof(req.token));
                    snprintf((char *)req.token, sizeof(req.token), "%s", "98765");
                    req.payload_type = PAYLOAD_TYPE_FLV;
                    encode_u2r_req_state(&req, _send_buf);

                    _state = FSStateReqSending;

                    break;
                }
            case FSStateReqSending:
                _state = FSStateRspReceiving;
                _disable_write();
                break;

            case FSStateWaitingData:
                _state = FSStateStreaming;
            case FSStateStartStreaming:
            case FSStateStreaming:
                {
                    static char * tmp_buf = new char[_send_buf->capacity()+sizeof(u2r_streaming)];
                    memset(tmp_buf, 0, _send_buf->capacity());
                    u2r_streaming * stream_pkg = (u2r_streaming *)tmp_buf;
                    stream_pkg->streamid = _stream_id.get_32bit_stream_id();
                    stream_pkg->payload_type = PAYLOAD_TYPE_FLV;

                    size_t need_size = _send_buf->capacity() - sizeof(proto_header);
                    int n = _flv_cache->pop_data((char *)stream_pkg->payload, need_size);
                    LOG_DEBUG(g_logger, "need: " << need_size << ". pop: " << n << " bytes data");
                    if (0 == n)
                    {
                        LOG_DEBUG(g_logger, "no data in cache between ant and sender");
                        _disable_write();
                        _state = FSStateWaitingData;
                    }
                    else
                    {
#ifdef DUMP_SENDER
                        int write_num = fwrite((char*)stream_pkg->payload, n, 1, sender_dump_fp);
                        if( write_num < 0)
                        {
                            LOG_WARN(g_logger, "FLVSender: write to sender dump file error: "<<strerror(errno));
                        }
#endif
                        stream_pkg->payload_size = n;

                        _state = FSStateStreaming;
                        if (0 != encode_u2r_streaming(stream_pkg, _send_buf))
                        {
                            LOG_ERROR(g_logger, "encode_u2r_streaming error");
                            assert(0);
                            _state = FSStateEnd;
                            break;
                        }
                        _last_active_time = time(NULL);
                    }

                    break;
                }
            default:
                // TODO
                break;
        }

    } while (_state != FSStateEnd && _send_buf->data_len() > 0);

}

void FLVSender::event_handler_impl_v2(const int fd, const short event)
{
    LOG_DEBUG(g_logger, "FLVSender::event_handler_impl_v2");
    if (this->_trigger_off)
    {
        this->_state = FSStateEnd;
    }

    if (event & EV_READ)
    {
        _read_handler_impl_v2(fd, event);
    }
    else if (event & EV_WRITE)
    {
        _write_handler_impl_v2(fd, event);
    }
    else
    {
        LOG_ERROR(g_logger, "event invalid, event: " << event);
    }
}

void FLVSender::_read_handler_impl_v2(const int fd, const short event)
{
    LOG_DEBUG(g_logger, "FLVSender::_read_handler_impl_v2");
    if (_receive_buf->read_fd(_sock) < 0)
    {
        LOG_WARN(g_logger, "FLVSender::_read_handler_impl_v2: read from receiver error. stream_id: " << _stream_id.unparse());
        _state = FSStateEnd;
        return;
    }

    _last_active_time = time(NULL);
    switch (_state)
    {
    case FSStateRspReceiving_v2:
    {
        u2r_rsp_state_v2 rsp;
        int ret = decode_u2r_rsp_state_v2(&rsp, _receive_buf);
        if (ret < 0)
        {
            LOG_DEBUG(g_logger, "_read_handler_impl_v2::decode_u2r_rsp_state error.");
            _state = FSStateEnd;
            return;
        }
        else if (ret == 0)
        {
            if (rsp.result == 0)
            {
                _state = FSStateStartStreaming_v2;
                _enable_write();
                if (_admin)
                {
                    _admin->sender_callback(_stream_id, SENDER_EVENT_STARTED, true);
                }
            }
            else
            {
                LOG_DEBUG(g_logger, "_read_handler_impl_v2::u2r_rsp_state_v2 return error. result: " << rsp.result);
                _state = FSStateEnd;
                return;
            }
        }
        else
        {
            // need more read
        }
        break;
    }
    default:
        // TODO
        break;
    }
}

void FLVSender::_write_handler_impl_v2(const int fd, const short event)
{
    LOG_DEBUG(g_logger, "_write_handler_impl_v2.");

    do
    {
        LOG_DEBUG(g_logger, "datasize: " << _send_buf->data_len());

        int date_len = _send_buf->data_len();

        if (date_len > 0)
        {
            int write_bytes = _send_buf->write_fd(_sock);
            if (write_bytes < 0)
            {
                // TODO 
                LOG_WARN(g_logger, "write to receiver error");
                _state = FSStateEnd;
                break;
            }
            else
            {
                _total_sent_bytes += write_bytes;
            }
        }

        if (_send_buf->data_len() > 0)
        {
            break;
        }

        _send_buf->try_adjust();

        switch (_state)
        {
        case FSStateConnecting:
        {
            char t;

            if (0 != read(_sock, &t, 0))
            {
                _state = FSStateEnd;
                LOG_WARN(g_logger, "connect to receiver error");
                return;
            }

            _state = FSStateConnected;
            LOG_DEBUG(g_logger, "connected to receiver.");
        }

        case FSStateConnected:
        {
            u2r_req_state_v2 req;
            req.version = 2;
            memcpy(req.streamid, _stream_id.uuid, sizeof(req.streamid));

            memset((char*)req.token, 0, sizeof(req.token));
            snprintf((char *)req.token, sizeof(req.token), "%s", "98765");
            req.payload_type = PAYLOAD_TYPE_FLV;
            encode_u2r_req_state_v2(&req, _send_buf);

            _state = FSStateReqSending_v2;

            break;
        }
        case FSStateReqSending_v2:
            _state = FSStateRspReceiving_v2;
            _disable_write();
            break;

        case FSStateWaitingData_v2:
            _state = FSStateStreaming_v2;
        case FSStateStartStreaming_v2:
        case FSStateStreaming_v2:
        {
            static char * tmp_buf = new char[_send_buf->capacity() + sizeof(u2r_streaming_v2)];
            memset(tmp_buf, 0, _send_buf->capacity());
            u2r_streaming_v2 * stream_pkg = (u2r_streaming_v2 *)tmp_buf;
            memcpy(stream_pkg->streamid, _stream_id.uuid, sizeof(stream_pkg->streamid));
            stream_pkg->payload_type = PAYLOAD_TYPE_FLV;

            size_t need_size = _send_buf->capacity() - sizeof(proto_header);
            int n = _flv_cache->pop_data((char *)stream_pkg->payload, need_size);
            LOG_DEBUG(g_logger, "need: " << need_size << ". pop: " << n << " bytes data");
            if (0 == n)
            {
                LOG_DEBUG(g_logger, "no data in cache between ant and sender");
                _disable_write();
                _state = FSStateWaitingData_v2;
            }
            else
            {
#ifdef DUMP_SENDER
                int write_num = fwrite((char*)stream_pkg->payload, n, 1, sender_dump_fp);
                if (write_num < 0)
                {
                    LOG_WARN(g_logger, "FLVSender: write to sender dump file error: " << strerror(errno));
                }
#endif
                stream_pkg->payload_size = n;


                _state = FSStateStreaming_v2;

                if (0 != encode_u2r_streaming_v2(stream_pkg, _send_buf))
                {
                    LOG_ERROR(g_logger, "encode_u2r_streaming error");
                    assert(0);
                    _state = FSStateEnd;
                    break;
                }
                _last_active_time = time(NULL);
            }
            break;
        }
        default:
            // TODO
            break;
        }

    } while (_state != FSStateEnd && _send_buf->data_len() > 0);
}

void FLVSender::_start_timer()
{
    _start_timer(0, 100000);
}

void FLVSender::_start_timer(uint32_t sec, uint32_t usec)
{
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = usec;

    levtimer_set(&_timer_event, timer_event_handler, this);
    event_base_set(_event_base, &_timer_event);
    levtimer_add(&_timer_event, &tv);
    _timer_enable = true;
}

void FLVSender::_stop_timer()
{
    LOG_DEBUG(g_logger, "FLVSender::_stop_timer");
    if (_timer_enable)
    {
        levtimer_del(&_timer_event);
        _timer_enable = false;
    }
}

int FLVSender::_idle_time()
{
    return time(NULL) - _last_active_time;
}

void FLVSender::on_timer(const int fd, const short event)
{
    _timer_enable = false;

    if (_idle_time() > 60)
    {
        LOG_WARN(g_logger, "idle time > 60, stop sender: stream id: " << _stream_id.unparse() );
        _stop(true);
        return;
    }
    if (_trigger_off)
    {
        LOG_INFO(g_logger, "_trigger_off, stop sender: stream id: " << _stream_id.unparse());
        _stop(true);
        return;
    }
    if (_state == FSStateEnd)
    {
        LOG_INFO(g_logger, "_state == FSStateEnd, stop sender: stream id: " << _stream_id.unparse());
        _stop(true);
        return;
    }

    if (_state == FSStateWaitingData || _state == FSStateWaitingData_v2)
    {
        _enable_write();
    }
    
    _start_timer();
}

void FLVSender::stop()
{
    LOG_DEBUG(g_logger, "FLVSender::stop");
    _trigger_off = true;
}

void FLVSender::_stop(bool restart)
{
    LOG_DEBUG(g_logger, "FLVSender::_stop");
    _stop_timer();

    if (_sock > 0)
    {
        if (this->_event_enable)
        {
            levent_del(&_event);
            this->_event_enable = false;
        }
        
        close(_sock);
        _sock = -1;
    }

    if (_admin)
    {
        _admin->sender_callback(_stream_id, SENDER_EVENT_STOPED, restart);
    }
    
    LOG_INFO(g_logger, "a sender stop. stream_id: " << _stream_id.unparse());
}

uint64_t FLVSender::get_total_sent_bytes()
{
    return _total_sent_bytes;
}
