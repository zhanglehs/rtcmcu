#ifndef __STREAM_MANAGER_H_
#define __STREAM_MANAGER_H_

#include <map>

#include "tcp_server.h"
#include "tcp_connection.h"
#include "event_loop.h"
#include "ip_endpoint.h"
#include "utils/logging.h"
#include "common/proto.h"
#include "utils/buffer.hpp"
#include "forward_manager_v3.h"
class StreamItem;

using namespace lcdn;
using namespace lcdn::net;
using namespace lcdn::base;

class StreamItem
{
public:
    StreamItem()
        : stream_id(0),
          last_ts(0),
          block_seq(0) {}
    StreamItem(const StreamItem& right)
    {
        this->stream_id = right.stream_id;
        this->start_time.tv_sec = right.start_time.tv_sec;
        this->start_time.tv_usec = right.start_time.tv_usec;
        this->last_ts = right.last_ts;
        this->block_seq = right.block_seq;
    }

    StreamItem& operator=(const StreamItem& right)
    {
        this->stream_id = right.stream_id;
        this->start_time.tv_sec = right.start_time.tv_sec;
        this->start_time.tv_usec = right.start_time.tv_usec;
        this->last_ts = right.last_ts;
        this->block_seq = right.block_seq;

        return *this;
    }

public:
    uint32_t stream_id;
    timeval start_time;
    uint32_t last_ts;
    uint64_t block_seq;
};

typedef std::map<uint32_t, StreamItem> StreamMap;

class StreamManager
{
public:
    StreamManager();
    ~StreamManager();
    void insert(const StreamItem& item);
    void update_if_existed(uint32_t stream_id, uint32_t last_ts, uint64_t block_seq);
    void erase(uint32_t stream_id);
    StreamMap& stream_map();
    void make_stream_manager_ready();
    bool is_stream_manager_ready();
private:
    StreamMap _stream_map;
    bool _is_stream_manager_ready;
};

class StreamListNotifier : public TCPConnection::Delegate
{
public:
    class KeepalivedTimerWatcher : public EventLoop::TimerWatcher
    {
    public:
        explicit KeepalivedTimerWatcher(StreamListNotifier* notifier)
            : _notifier(notifier) {}
        ~KeepalivedTimerWatcher() {}
        virtual void on_timer() { _notifier->_check_connection(); }
    private:
        StreamListNotifier* _notifier;
    };
public:
    StreamListNotifier(EventLoop* loop, InetAddress& listen_addr,
                       std::string name, StreamManager* stream_manager, ForwardServerManager *forward_manager);
    ~StreamListNotifier();
    void start();
    void notify_stream_list(TCPConnection* connection);
    void notify_stream_start(StreamItem& item);
    void notify_stream_close(uint32_t stream_id);

private:
    bool is_frame(Buffer* inbuf, proto_header& header);
    void send_to_all(Buffer*);
    void send(Buffer* buffer, TCPConnection *connection);
    void handle_frame(Buffer* inbuf, proto_header& header, TCPConnection* connection);
    virtual void on_connect(TCPConnection*);
    virtual void on_read(TCPConnection*);
    virtual void on_write_completed(TCPConnection*);
    virtual void on_close(TCPConnection*);
    void _check_connection();

private:
    typedef std::map<uint32_t, TCPConnection*> ConnectionMap;

    static const int connection_timeout = 10; // seconds
    static const int64_t keepalived_timer_interval = 3 * 1000; // mill seconds
    static const int input_buffer_size = 1024 * 1024;
    static const int output_buffer_size = 1024 * 1024;

    EventLoop *_loop;
    TCPServer _tcp_server;
    ConnectionMap _conn_map;
    StreamManager* _stream_manager;

    ForwardServerManager* _forward_manager;
    KeepalivedTimerWatcher _keepalived_watcher;
};

#endif // __STREAM_MANAGER_H_

