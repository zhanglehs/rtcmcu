#ifndef _BASE_NET_REACTOR_H_
#define _BASE_NET_REACTOR_H_
#include <unistd.h>
#include <appframe/array_object_pool.hpp>
#include <xthread/runnable.hpp>
#include "selector.hpp"
#include "serversocketchannel.hpp"
#include "socketchannel.hpp"
#include "biniarchive.hpp"
#include "imessageprocessor.hpp"
#include "connectioninfo.hpp"
#include "connectioneventlistener.hpp"

using namespace std;

class Acceptor;


class Reactor : public ConnectionEventListener
{
    friend class Acceptor;

public:
    enum
    {
     DEFAULT_ACCECPT_ONCE = 128,
     DEFAULT_BACKLOG = 128,
     DEFAULT_SELECT_TIMEOUT  = 10,        //10 ms
     DEFAULT_MAX_CONNECTIONS = 128,  
    };

    typedef ArrayObjectPool<ConnectionInfo> ConnectionInfoPool;
    
    Reactor(int port, IMessageProcessor& processor,
            int max_connections = DEFAULT_MAX_CONNECTIONS,
            int accept_once = DEFAULT_ACCECPT_ONCE,
            int backlog = DEFAULT_BACKLOG,
            int select_timeout=DEFAULT_SELECT_TIMEOUT);
    Reactor(char* ip, int port, IMessageProcessor& processor,
            int max_connections = DEFAULT_MAX_CONNECTIONS,
            int accept_once = DEFAULT_ACCECPT_ONCE,
            int backlog = DEFAULT_BACKLOG,
            int select_timeout=DEFAULT_SELECT_TIMEOUT);

    virtual ~Reactor()
    {
    }
    int init();
    int poll();
    void wakeup();
    void connection_event(ConnectionEvent& e);

private:
    int accept_pending_connections(SelectionKey& key);
    int register_new_channels();

private:
    ConnectionInfoPool       _connection_pool;
    Selector* _selector;
    InetSocketAddress _server_addr;
    ServerSocketChannel* _server_socket;
    SelectionKey* _server_sk;
    Acceptor*         _acceptor;

    IMessageProcessor* _processor;
    ConnectionInfoList _to_register;
    ConnectionInfoList _to_unregister;

    int   _accept_once;
    int   _backlog;
    int   _select_timeout;

};

inline
void Reactor::wakeup()
{
    _selector->wakeup();
}


class Acceptor : public Attachment
{
private:
    Reactor* _reactor;
public:
    Acceptor(Reactor& reactor)
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


#endif

