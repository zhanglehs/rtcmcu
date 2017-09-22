#ifndef _BASE_NET_ACTIVE_REACTOR_H_
#define _BASE_NET_ACTIVE_REACTOR_H_

#include <unistd.h>
#include <ext/hash_map>
#include "appframe/array_object_pool.hpp"
#include "selector.hpp"
#include "serversocketchannel.hpp"
#include "socketchannel.hpp"
#include "biniarchive.hpp"
#include "imessageprocessor.hpp"
#include "connectioninfo.hpp"
#include "connectioneventlistener.hpp"
#include "selectorprovider.hpp"

using namespace std;
using namespace __gnu_cxx;

namespace __gnu_cxx
{
template<> 
struct hash<SelectableChannel*>
{
    size_t operator()(const SelectableChannel* __s) const
        { return  (size_t)__s;  }



};
}

class Acceptor;
class Connector;

class ActiveReactor : public ConnectionEventListener
{
    friend class Acceptor;
    friend class Connector;
    
public:
    struct  ConnectorInfo
    {
        IMessageProcessor* _processor;
        InetSocketAddress   _peer_addr;
        ConnectionInfo*        _connection_info;
    };
        
    enum
    {
     DEFAULT_ACCECPT_ONCE = 128,
     DEFAULT_BACKLOG = 128,
     DEFAULT_SELECT_TIMEOUT  = 10,        //10 ms
     DEFAULT_MAX_CONNECTIONS  = 128,        //10 ms     
    };

    typedef ArrayObjectPool<ConnectionInfo> ConnectionInfoPool;
    
    static const uint32_t MAX_ACCEPTOR_NUM=10;
    static const uint32_t MAX_CONNECTOR_NUM=30;
    
    ActiveReactor(int select_timeout=DEFAULT_SELECT_TIMEOUT,
                                int max_connections = DEFAULT_MAX_CONNECTIONS,
                                int accept_once = DEFAULT_ACCECPT_ONCE,
                                 int backlog = DEFAULT_BACKLOG
                                 );

    virtual ~ActiveReactor();
    
    int add_acceptor(const char* ip, int port, IMessageProcessor& processor);
    int add_acceptor(int port,  IMessageProcessor& processor);    
    int add_connector(const char* peer_ip, int peer_port,  IMessageProcessor& processor);
    int async_connect(ConnectorInfo& connector_info);
    
    int init();
    int poll();
    void wakeup();
    void connection_event(ConnectionEvent& e);

private:
    int accept_pending_connections(SelectionKey& key);
    int finish_connect(SelectionKey& key);
    int register_new_channels();

private:

    typedef hash_set<IMessageProcessor*>                                 ProcessorSet;
    typedef hash_map<SelectableChannel*, IMessageProcessor*> SocketProcessorMap;
    typedef hash_map<SelectableChannel*, ConnectorInfo>         SocketConnectorMap;

    ConnectionInfoPool       _connection_pool;    
    Selector* _selector;

    ProcessorSet                 _processors;
    SocketProcessorMap       _acceptor_sockets;
    SocketConnectorMap       _connector_sockets;
 
    Acceptor*         _acceptor;
    Connector*       _connector;
    
    ConnectionInfoList _to_register;
    ConnectionInfoList _to_unregister;

    int   _accept_once;
    int   _backlog;
    int   _select_timeout;

};

inline
void ActiveReactor::wakeup()
{
    _selector->wakeup();
}

inline
int ActiveReactor::add_acceptor(int port,  IMessageProcessor& processor)
{
    return add_acceptor(NULL,  port, processor);
}



class Acceptor : public Attachment
{
private:
    ActiveReactor* _reactor;
public:
    Acceptor(ActiveReactor& reactor)
            : _reactor(&reactor)
    {
    }
    int run(SelectionKey& key);
};

inline
int Acceptor::run(SelectionKey& key)
{
    return  _reactor->accept_pending_connections(key);
}

class Connector : public Attachment
{
private:
    ActiveReactor* _reactor;
public:
    Connector(ActiveReactor& reactor)
            : _reactor(&reactor)
    {
    }
    int run(SelectionKey& key);
};

inline
int Connector::run(SelectionKey& key)
{
    return  _reactor->finish_connect(key);
}
#endif

