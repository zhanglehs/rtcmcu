/**
 * \file
 * \biref xsocket/IMessageProcessor 的一个子类
 */
#ifndef _DEFAULT_MESSAGE_PROCESSOR_HPP_
#define _DEFAULT_MESSAGE_PROCESSOR_HPP_

#include <stdint.h>
#include <ext/hash_map>
#include <ext/hash_set>
#include "imessageprocessor.hpp"
#include "connectioninfo.hpp"
#include "datagramfactory.hpp"
#include "thread_channel.hpp"
#include "xsocket/imessageprocessor.hpp"

using namespace std;
using namespace _gnu_cxx;


typedef hash_map<uint32_t, ConnectionInfo* > ConnectionMap;
typedef hash_set<ConnectionInfo* > ConnectionSet;

struct ChannelMessge
{
    uint32_t conn_id;    //连接id
    ByteBuffer* buf;
};

typedef  ThreadChannel<ChannelMessge>   ThreadChannel;

class DefaultMessageProcessor
            :public IMessageProcessor
{
public:
    enum
    {
        DEFAULT_MAX_CONNECTIONS = 1024,
        DEFAULT_WORKER_THREAD_NUM = 1,
        DEFAULT_MAX_RECV_ONCE           = 1024,
    };

public:
    DefaultMessageProcessor(uint32_t worker_thread_num = DEFAULT_WORKER_THREAD_NUM,
                            uint32_t max_connetions = DEFAULT_MAX_CONNECTIONS);

    virtual ~DefaultMessageProcessor(){}
    int    init();
    void start();
    int    frame(ByteBuffer& bb);
    void accept(ConnectionInfo& ci);
    void remove(ConnectionInfo& ci);
    void process_input(ConnectionInfo& conn, ByteBuffer& packet);
    int    process_output(uint32_t timeout = 0);

private:
    DefaultDatagramFactory _df;
    ConnectionSet _connections;
    ConnectionMap _id_connections;
    uint32_t  _max_connetions;

    uint32_t  _worker_thread_num;
    ThreadChannel* _thread_channels;
    Thread* _worker_threads;
    ThreadChannel::Accessor * _accessors;
};

inline
int  DefaultMessageProcessor::frame(ByteBuffer& bb)
{
    return _df.frame(bb);
}

inline
void DefaultMessageProcessor::accept(ConnectionInfo& ci)
{
    _connections.insert(&ci);
    _id_connections.insert(ConnectionMap::value_type(ci.get_id(), &ci));
}

inline
void DefaultMessageProcessor::remove(ConnectionInfo& ci)
{
    _connections.erase(&ci);
    _id_connections.erase(ci.get_id());

}
#endif
