#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include "biniarchive.hpp"
#include "selectorprovider.hpp"
#include "reactor.hpp"


Reactor::	Reactor(int port, IMessageProcessor& processor, int max_connections,
                  int accept_once, int backlog, int select_timeout)
        :_connection_pool(max_connections),
        _selector(NULL),
        _server_addr(0, port),
        _server_socket(NULL),
        _server_sk(NULL),
        _acceptor(NULL),
        _processor(&processor),
        _accept_once(accept_once),
        _backlog(backlog),
        _select_timeout(select_timeout)
{

}

Reactor::	Reactor(char* ip, int port, IMessageProcessor& processor, int max_connections,
                  int accept_once, int backlog, int select_timeout)
        :_connection_pool(max_connections),
        _selector(NULL),
        _server_addr(ip, port),
        _server_socket(NULL),
        _server_sk(NULL),
        _acceptor(NULL),
        _processor(&processor),
        _accept_once(accept_once),
        _backlog(backlog),
        _select_timeout(select_timeout)
{

}

int Reactor::init()
{
    int rv;

    rv = _connection_pool.init();
    if(rv < 0)
    {
        return -1;
    }
    
    //_selector = Selector::open();
    _selector = SelectorProvider::provider().open_selector("epoll");
	rv = _selector->init(_connection_pool.capacity());
	if (rv < 0)
	{
		return -1;
	}
    _server_socket = ServerSocketChannel::open();
    _acceptor = new Acceptor(*this);

    _server_socket->send_buf_size(1024*1024);
    _server_socket->recv_buf_size(1024*1024);
    rv = _server_socket->bind(_server_addr, _backlog);
    if (rv < 0)
    {
        return -1;
    }
    _server_socket->configure_blocking(false);

    //向selector注册该channel
    _server_sk = _server_socket->enregister(*_selector, SelectionKey::OP_ACCEPT);

    //利用sk的attache功能绑定Acceptor 如果有事情，触发Acceptor

    _server_sk->attach(_acceptor);

    return 0;
}


int Reactor::poll()
{
    register_new_channels();
    int num_keys = _selector->select(_select_timeout);

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

    _processor->process_output();
    
    return num_keys;
}


int Reactor::accept_pending_connections(SelectionKey& key)
{
    int rv =0;

    int i = 0;


    ServerSocketChannel* ready_channel = (ServerSocketChannel*)key.channel();

    for (; i < _accept_once; i++)
    {
        SocketChannel  new_channel;    
        rv = ready_channel->accept(new_channel);

        if (rv == 0)
        {
            ConnectionInfo* conn = _connection_pool.construct();
            if(conn == NULL)
            {
                new_channel.close();
                return i;
            }        
            conn->open(new_channel, *_processor);
            _to_register.push_back(conn);
        }
        else
        {
            break;
        }
    }

    return i;
}

void Reactor::connection_event(ConnectionEvent& e)
{
    if(ConnectionEvent::CONNECTION_CLOSED == e.getEventCode())
    {
        _to_unregister.push_back(&e.getConnection());
    }
}

int Reactor::register_new_channels()
{
    //注册新的channel
    for (ConnectionInfoList::iterator iter = _to_register.begin(); iter != _to_register.end(); iter++)
    {
        ConnectionInfo* conn = *iter;
        SocketChannel& incoming_channel  = conn->get_channel();

        incoming_channel.configure_blocking(false);
        incoming_channel.tcp_nodelay();

        SelectionKey* sk = incoming_channel.enregister(*_selector, 0, conn);

        //同时将SelectionKey标记为可读，以便读取。
        sk->interest_ops(SelectionKey::OP_READ);
        conn->add_event_listener(*this);
    }
    _to_register.clear();

    //删除关闭的channel
    for (ConnectionInfoList::iterator iter = _to_unregister.begin(); iter != _to_unregister.end(); iter++)
    {
        ConnectionInfo* ci = *iter;
        _connection_pool.destroy(ci);
    }

    _to_unregister.clear();

    return 0;
}


