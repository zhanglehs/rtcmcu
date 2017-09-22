#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <iostream>
#include <new>
#include "biniarchive.hpp"
#include "selectorprovider.hpp"
#include "active_reactor.hpp"

using namespace std;

ActiveReactor::ActiveReactor(int select_timeout, int max_connections, int accept_once, int backlog)
        :_connection_pool(max_connections),
        _selector(NULL),    
        _acceptor(NULL),
        _connector(NULL),
        _accept_once(accept_once),
        _backlog(backlog),
        _select_timeout(select_timeout)
{
        _acceptor = new Acceptor(*this);
        _connector  = new Connector(*this);
        
        //_selector = Selector::open();     
        _selector = SelectorProvider::provider().open_selector("epoll");
}


ActiveReactor::~ActiveReactor()
{
    if(_acceptor)
    {
        delete _acceptor;
        _acceptor = NULL;
    }

    if(_connector)
    {
        delete _connector;
        _connector = NULL;
    }
    
    if(_selector)
    {
        delete _selector;
        _selector = NULL;
    }
}

    
int ActiveReactor::add_acceptor(const char* ip, int port, IMessageProcessor& processor)
{
    int rv;

    if(_acceptor_sockets.size() >= MAX_ACCEPTOR_NUM)
    {
        return -1;
    }
    
    InetSocketAddress server_addr(ip, port);


    ServerSocketChannel* ssc =  ServerSocketChannel::open();

    ssc->send_buf_size(1024*1024);
    ssc->recv_buf_size(1024*1024);

    rv = ssc->bind(server_addr, _backlog);
    if (rv < 0)
    {
        return -1;
    }
    ssc->configure_blocking(false);

    //向selector注册该channel
    SelectionKey* server_sk =  ssc->enregister(*_selector, SelectionKey::OP_ACCEPT);

    //利用sk的attache功能绑定Acceptor 如果有事情，触发Acceptor

    server_sk->attach(_acceptor);

    _acceptor_sockets.insert(SocketProcessorMap::value_type((SelectableChannel*)ssc, &processor));
    _processors.insert(&processor);

    return _acceptor_sockets.size();
    
}




int ActiveReactor::add_connector(const char* peer_ip, int peer_port,  IMessageProcessor& processor)
{
    if(_connector_sockets.size() >= MAX_CONNECTOR_NUM)
    {
        return -1;
    }
    
    InetSocketAddress    serveraddr(peer_ip, peer_port);

    ConnectorInfo    connector_info;
    connector_info._processor = &processor;   
    connector_info._peer_addr = serveraddr;
    connector_info._connection_info = _connection_pool.construct();
    if(connector_info._connection_info == NULL)
    {
        return -1;
    }    
    _processors.insert(& processor);

    SocketChannel* sc = &connector_info._connection_info->get_channel();
  
    _connector_sockets.insert(SocketConnectorMap::value_type( (SelectableChannel*)sc, connector_info));  
    
    return async_connect(connector_info);
}


int ActiveReactor::async_connect(ConnectorInfo& connector_info)
{
    SocketChannel new_sc(&SelectorProvider::provider());
    connector_info._connection_info->~ConnectionInfo();
    new(connector_info._connection_info)ConnectionInfo();
    connector_info._connection_info->channel(new_sc);
    connector_info._connection_info->add_event_listener(*this);

    SocketChannel* sc = &connector_info._connection_info->get_channel();
    sc->configure_blocking(false);
    sc->send_buf_size(1024*1024);
    sc->recv_buf_size(1024*1024);    
    sc->connect(connector_info._peer_addr);
    //向selector注册该channel
    SelectionKey* sk  =sc->enregister(*_selector, SelectionKey::OP_CONNECT);
    
    sk->attach(_connector);
    //cout<<"async connect send "<<endl;
    return _connector_sockets.size();    
}


int ActiveReactor::init()
{
    int rv;
    rv = _selector->init(_connection_pool.capacity()+MAX_ACCEPTOR_NUM+MAX_CONNECTOR_NUM);
    if (rv < 0)
    {
        return -1;
    }
    rv = _connection_pool.init();
    if (rv < 0)
    {
        return -1;
    }
    
    return 0;
}


int ActiveReactor::poll()
{
    
    register_new_channels();
    
    int num_keys=0;
    if(_select_timeout==0)
    {
        num_keys = _selector->select_now();
    }
    else
    {
        num_keys = _selector->select(_select_timeout);
    }

    
    //如果有我们注册的事情发生了，它的传回值就会大于0
    if (num_keys > 0)
    {
        SelectionKey* selected_key = NULL;
        while((selected_key = _selector->next()))
        {
            Attachment* r = (Attachment*)(selected_key->attachment());
            if (r != NULL)
            {
                r->run(*selected_key);
            }
        }
    }

    ProcessorSet::iterator iter = _processors.begin();
    for(;iter!=_processors.end(); iter++)
    {
        (*iter)->process_output();
    }

    return num_keys;
}


int ActiveReactor::accept_pending_connections(SelectionKey& key)
{
    int rv =0;
    int i   = 0;


    ServerSocketChannel* ready_channel = (ServerSocketChannel*)key.channel();    
    
    for (; i < _accept_once; i++)
    {
        SocketChannel new_channel;
        rv = ready_channel->accept(new_channel);

        if (rv == 0)
        {
            ConnectionInfo* conn = _connection_pool.construct();
            if(conn == NULL)
            {
                new_channel.close();
                return i;
            }       

            SocketProcessorMap::iterator iter = _acceptor_sockets.find((SelectableChannel*)ready_channel);
            
            conn->bind(new_channel, *(iter->second));

            _to_register.push_back(conn);
        }
        else
        {
            break;
        }        
    }

    return i;

  
}

int ActiveReactor::finish_connect(SelectionKey& key)
{
    SocketChannel* sc = (SocketChannel*)key.channel();
    SocketConnectorMap::iterator iter = _connector_sockets.find((SelectableChannel*)sc);    

    assert(iter != _connector_sockets.end());
        
    if(sc->finish_connect() )
    {
        sc->configure_blocking(false);
        sc->tcp_nodelay();

        key.interest_ops(SelectionKey::OP_READ);

        ConnectionInfo* conn = iter->second._connection_info;    
       conn->open(*(iter->second._processor));
       conn->add_event_listener(*this);        

        key.attach(conn);

        return 0;
    }
    else
    {
        iter->second._connection_info->close();
        async_connect(iter->second);
        return -1;
    }

}

void ActiveReactor::connection_event(ConnectionEvent& e)
{
    if(ConnectionEvent::CONNECTION_CLOSED == e.getEventCode())
    {
        _to_unregister.push_back(&e.getConnection());
    }
}

int ActiveReactor::register_new_channels()
{
    //注册新的channel
    for (ConnectionInfoList::iterator iter = _to_register.begin(); iter != _to_register.end(); iter++)
    {
        ConnectionInfo* conn = *iter;
        SocketChannel& incoming_channel  = conn->get_channel();

        incoming_channel.configure_blocking(false);
        incoming_channel.tcp_nodelay();
   
        SelectionKey* sk = incoming_channel.enregister(*_selector, 0, conn);

    if(NULL != sk)
    {
            //同时将SelectionKey标记为可读，以便读取。
            sk->interest_ops(SelectionKey::OP_READ);
            conn->open();
            conn->add_event_listener(*this);
    }
    else
    {
        //just close it
        conn->close();
    }
    }
    _to_register.clear();

    //删除关闭的channel
    for (ConnectionInfoList::iterator iter = _to_unregister.begin(); iter != _to_unregister.end(); iter++)
    {
        ConnectionInfo* ci = *iter;
        //如果该channel在_connector_sockets中，自动重连
        SelectableChannel* channel = &(ci->get_channel());

        SocketConnectorMap::iterator iter_connector = _connector_sockets.find(channel);
        if(iter_connector != _connector_sockets.end())
        {
            //reconnect
            async_connect(iter_connector->second);
        }
        else
        {
            _connection_pool.destroy(ci);
        }
        
    }

    _to_unregister.clear();

    return 0;
}


