#ifndef _CONNECTION_INFO_
#define  _CONNECTION_INFO_

#include <stdint.h>
#include <list>
#include <ext/hash_map>
#include <ext/hash_set>
#include "attachment.hpp"
#include "bytebuffer.hpp"
#include "imessageprocessor.hpp"
#include "connectionevent.hpp"
#include "connectioneventlistener.hpp"
#include "bytechannel.hpp"
#include "selectionkey.hpp"
#include "socketchannel.hpp"


using namespace __gnu_cxx;
using namespace std;

class ConnectionInfo;

typedef list<ConnectionInfo*>   ConnectionInfoList;

class ConnectionInfo : public Attachment
{
    friend class Reactor;
    friend class ActiveReactor;    
public:
    enum
    {
        MAX_READ=1048576+1024*128,     // 1 M
        FLUSH_BUFFER_THRESHOLD = 1048576, // 1 M
        PACKET_PROCESSED_ONCE   = 100,
    };

    ConnectionInfo();
    ConnectionInfo(SocketChannel& channel, IMessageProcessor& p);

    virtual ~ConnectionInfo();

public:
    int   run(SelectionKey& key);
    int   bind(SocketChannel& channel, IMessageProcessor& p);
    int   open(SocketChannel& channel, IMessageProcessor& p);
    int   open(IMessageProcessor& p); 
    int   open();
    
    bool is_open();
    void close();

    bool ready_to_write();
    void read(ByteChannel& channel, SelectionKey& key);
    int   write_buf_remaining();
    int   write(ByteBuffer& bb);

    int  flush();
    bool flushable();

    int packet_processed_once();
    int    packet_processed_once(int num);
    void process_data();

    void add_event_listener(ConnectionEventListener& l);
    void remove_event_listener(ConnectionEventListener& l);

    int get_id();

    int get_timer_id();
    int set_timer_id(int timer_id);
    
    SocketChannel& channel(SocketChannel& channel);
    SocketChannel& get_channel();
    int get_read_bytes();
    int reset_read_bytes();    
    int get_write_bytes();

    static  uint32_t allocate_process_unique_id();

private:
    void send_event(ConnectionEvent& event);
    void send_event(int eventCode);

    void pre_process_data();
    void post_process_data();

    void efficient_compact(ByteBuffer& bb);

private:
    uint32_t _id;
    int         _timer_id;
    uint32_t _total_read_bytes;
    uint32_t _total_write_bytes;
    uint32_t _packet_processed_once;

    bool _open;
    SocketChannel _channel;

    char rbuf[MAX_READ];
    char wbuf[MAX_READ];

    ByteBuffer _read_buffer;
    ByteBuffer _write_buffer;

    ConnectionEventListernerSet _event_handlers;

    // statics
    static long _next_id;

    // the message processor. very important.
    IMessageProcessor* _proc;

};



inline
int ConnectionInfo::get_id()
{
    return _id;
}

inline
int ConnectionInfo::get_timer_id()
{
    return _timer_id;
}

inline
int ConnectionInfo::set_timer_id(int timer_id)
{
    return _timer_id = timer_id;
}

inline
SocketChannel& ConnectionInfo::channel(SocketChannel& channel)
{
    return _channel = channel;
}
    
inline
SocketChannel& ConnectionInfo::get_channel()
{
    return _channel;
}

inline
int ConnectionInfo::get_read_bytes()
{
    return _total_read_bytes;
}

 
inline
int ConnectionInfo::reset_read_bytes()
{
    return (_total_read_bytes=0);
}

inline
int ConnectionInfo::get_write_bytes()
{
    return _total_write_bytes;
}

inline
int ConnectionInfo::write_buf_remaining()
{
    return _write_buffer.remaining();
}



inline
int    ConnectionInfo::packet_processed_once()
{
    return _packet_processed_once;
}

inline
int    ConnectionInfo::packet_processed_once(int num)
{
    return _packet_processed_once = num;
}
    
inline
uint32_t ConnectionInfo::allocate_process_unique_id()
{
    return ++_next_id;
}

inline
bool ConnectionInfo::flushable()
{
    return  (_write_buffer.position() > 0);
}
    

namespace __gnu_cxx
{
template<> 
struct hash<ConnectionInfo*>
{
    size_t operator()(const ConnectionInfo* __s) const
        { return  (size_t)__s;  }



};
}

typedef hash_map<uint32_t, ConnectionInfo* > ConnectionMap;
typedef hash_set<ConnectionInfo*> ConnectionSet;

#endif

