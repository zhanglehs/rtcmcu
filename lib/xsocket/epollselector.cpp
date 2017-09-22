#include "net.hpp"
#include "ioexception.hpp"
#include "epollselector.hpp"
#include <iostream>

using namespace std;

EPollSelector::EPollSelector()
        : _selected_channel_num(0),
        _selector_open(true),
        _iterator_pos(-1)

{
}

EPollSelector::~EPollSelector()
{
    close();
}

int   EPollSelector::init(int max_channels)
{
    int rv = 0;
    
    _capacity   = max_channels;
    
    int fdes[2];

    ::pipe(fdes);

    _fd0 = fdes[0];
    _fd1 = fdes[1];

    rv = _epoll_wrapper.init(_capacity);
    if(rv < 0)
    {
        return -1;
    }
            
    _epoll_wrapper.init_interrupt(_fd0, _fd1);

    rv = _selection_key_pool.init(_capacity);
    if(rv < 0)
    {
        return -1;
    }

    //_channel_array.set_empty_key(-1);
    //_channel_array.set_deleted_key(-2);
    
    return 0;
}


int EPollSelector::select()
{
    return select(0);
}

int EPollSelector::select(long timeout)
{
    if (timeout < 0)
    {
        return -1;
    }

    if (!is_open())
    {
        return -1;
    }

    return do_select((timeout == 0) ? -1 : timeout);
}




int EPollSelector::select_now()
{
    if (!is_open())
    {
        return -1;
    }

    return do_select(0);
}

bool  EPollSelector::has_next()
{
    if(_selected_channel_num <= 0)
        return false;

    //第一次访问
    if(_iterator_pos == -1)
    {
        _iterator_pos = 0;
        proc_interrupt();
        if(_iterator_pos < _selected_channel_num)
        {
            _iterator_pos--;
            return true;
        }
        else
        {
            _iterator_pos--;
            return false;
        }
    }

    //不是第一次访问
    if(_iterator_pos < _selected_channel_num -1)
    {
        proc_interrupt();
        if(_iterator_pos < _selected_channel_num -1)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

SelectionKey*  EPollSelector::first()
{
    if(_selected_channel_num <= 0)
    {
        return NULL;
    }

    _iterator_pos = 0;

    proc_interrupt();

    if(_iterator_pos > _selected_channel_num -1)
    {
        return NULL;
    }

    return selected_key(_iterator_pos);
}

SelectionKey*  EPollSelector::next()
{
    if(_selected_channel_num <= 0)
    {
        return NULL;
    }

    if(_iterator_pos == -1)
    {
        _iterator_pos = 0;
    }
    else if(_iterator_pos < _selected_channel_num -1)
    {
        _iterator_pos++;
    }
    else
    {
        return NULL;
    }

    proc_interrupt();

    if(_iterator_pos > _selected_channel_num -1)
    {
        return NULL;
    }

    return selected_key(_iterator_pos);

}



void EPollSelector::cancel(SelectionKey& key)
{
    unsigned int i = (unsigned int)key.get_index();

    _epoll_wrapper.ctl_del(key.channel()->get_fd());
    _channel_array.erase(i);
    key.set_index(-1);

    _selection_key_pool.destroy(&key);
}


SelectionKey* EPollSelector::enregister(SelectableChannel& ch, int ops, Attachment* attachment)
{
    if (_channel_array.size() >= (unsigned int)_capacity)
    {
        return NULL;
    }

    SelectionKey* k = _selection_key_pool.construct();

    new(k)SelectionKey(ch, *this);
    k->attach(attachment);

    _channel_array.insert(dense_hash_map<int, SelectionKey*>::value_type(k->channel()->get_fd(), k));
    k->set_index(k->channel()->get_fd());

    _epoll_wrapper.ctl_add(k->channel()->get_fd(), EPOLLIN);

    k->interest_ops(ops);

    return k;
}

void EPollSelector::deregister(SelectionKey& key)
{

    key.channel()->remove_key(key);

}

void EPollSelector::put_event_ops(SelectionKey& sk, int ops)
{
    _epoll_wrapper.ctl_mod(sk.channel()->get_fd(), ops);
}

Selector& EPollSelector::wakeup()
{
    if (!_interrupt_triggered)
    {
        _epoll_wrapper.interrupt();
        _interrupt_triggered = true;
    }
    return *this;
}

void EPollSelector::close()
{
    if (!_selector_open)
    {
        return;
    }
    
    _selector_open = false;
    wakeup();

    // Deregister channels
    //dense_hash_map<int, SelectionKey* >::iterator ch_iter;
    hash_map<int, SelectionKey* >::iterator ch_iter;
    for (ch_iter = _channel_array.begin(); ch_iter != _channel_array.end(); ch_iter++)
    {
        SelectionKey* ski = ch_iter->second;
        ski->set_index(-1);
        _selection_key_pool.destroy(ski);
        deregister(*ski);
    }

    _channel_array.clear();
    
    close_interrupt();

}

int EPollSelector::do_select(long timeout)
{
    /*
    if (_channel_array.size() == 0)
    {
        return -1;
    }
    */

	
    _selected_channel_num = _epoll_wrapper.wait(timeout);


    _iterator_pos = -1;
    return _selected_channel_num;
}

void EPollSelector::close_interrupt()
{
    _epoll_wrapper.ctl_del(_fd0);

    Net::close(_fd0);
    Net::close(_fd1);

    _fd0 = -1;
    _fd1 = -1;

    _epoll_wrapper.close();
}


int EPollSelector::translate_ready_ops(int ops)
{
    int new_ops = 0;
    if ((ops & EPOLLIN) != 0)
        new_ops |= Selector::OP_READ ;
    if ((ops & EPOLLOUT) != 0)
        new_ops |= Selector::OP_WRITE;
    if ((ops & EPOLLERR) != 0)
        new_ops |= Selector::OP_ERR;
    if ((ops & EPOLLHUP) != 0)
        new_ops |= Selector::OP_HUP;
    if ((ops & EPOLLET) != 0)
        new_ops |= Selector::OP_ET ;
    return new_ops;
}

int EPollSelector::translate_interest_ops(int ops)
{
    int new_ops = 0;
    if ((ops & Selector::OP_READ) != 0)
        new_ops |= EPOLLIN;
    if ((ops & Selector::OP_WRITE) != 0)
        new_ops |= EPOLLOUT;
    if ((ops & Selector::OP_ERR) != 0)
        new_ops |= EPOLLERR;
    if ((ops & Selector::OP_HUP) != 0)
        new_ops |= EPOLLHUP;
    if ((ops & Selector::OP_ET) != 0)
        new_ops |= EPOLLET;

    return new_ops;
}





