#include "net.hpp"
#include "ioexception.hpp"
#include "pollselector.hpp"
#include <iostream>


using namespace std;

PollSelector::PollSelector()
        :_selector_open(true),
        _total_channels(1),
        _channel_offset(1),
        _selected_channel_num(0),
        _iterator_pos(0)
{

}

PollSelector::~PollSelector()
{
    close();
}

int   PollSelector::init(int max_channels)
{
    int rv = 0;
    _capacity   = max_channels + _channel_offset;
    
    int fdes[2];

    ::pipe(fdes);

    _fd0 = fdes[0];
    _fd1 = fdes[1];

    _poll_wrapper.init(_capacity);
    _poll_wrapper.init_interrupt(_fd0, _fd1);

    rv = _selection_key_pool.init(_capacity);
    if(rv < 0)
    {
        return -1;
    }
    
    _channel_array = new SelectionKey*[_capacity];

    return 0;
}


void PollSelector::close()
{
    if (!_selector_open)
    {
        return;
    }
    _selector_open = false;
    wakeup();

    // Deregister channels
    for (int i = _channel_offset; i < _total_channels; i++)
    {
        SelectionKey* ski = _channel_array[i];
        ski->set_index(-1);
        deregister(*ski);
    }

    close_interrupt();
    _poll_wrapper.free();
    _total_channels = 0;

}


void PollSelector::cancel(SelectionKey& k)
{
    int i = k.get_index();

    if (i != _total_channels - 1)
    {
        // Copy end one over it
        SelectionKey* end_channel = _channel_array[_total_channels - 1];
        _channel_array[i] = end_channel;
        end_channel->set_index(i);
        _poll_wrapper.release(i);
        PollArrayWrapper::replace_entry(_poll_wrapper, _total_channels - 1, _poll_wrapper, i);
    }
    else
    {
        _poll_wrapper.release(i);
    }

    _total_channels--;
    _poll_wrapper._total_channels--;

    _selection_key_pool.destroy(&k);
}


SelectionKey* PollSelector::enregister(SelectableChannel& ch, int ops, Attachment* attachment)
{
    // Check to see if the array is large enough
    if (_total_channels >= _capacity)
    {
        return NULL;
    }

    SelectionKey* k = _selection_key_pool.construct();
    _channel_array[_total_channels] =  k;
    new(k)SelectionKey(ch, *this);
    k->attach(attachment);
    k->set_index(_total_channels);
    _poll_wrapper.add_entry(*k->channel());
    _total_channels++;

    k->interest_ops(ops);

    return k;
}


void PollSelector::deregister(SelectionKey& key)
{
    key.channel()->remove_key(key);
}

void PollSelector::put_event_ops(SelectionKey& sk, int ops)
{
    _poll_wrapper.put_event_ops(sk.get_index(), ops);
}


int PollSelector::do_select(long timeout)
{
    if (_total_channels == 1)
    {
        return -1;
    }

    _selected_channel_num = _poll_wrapper.poll(_total_channels, 0, timeout);

    if (_poll_wrapper.get_revent_ops(0) != 0)
    {
        _selected_channel_num--;
        // Clear the wakeup pipe
        _poll_wrapper.put_revent_ops(0, 0);
        Net::drain(_fd0);
        _interrupt_triggered = false;

    }

    _iterator_pos = 0;

    return _selected_channel_num;
}

bool  PollSelector::has_next()
{
    int i;

    for ( i = _iterator_pos + 1; i < _total_channels; i++)
    {
        int returned_ops = _poll_wrapper.get_revent_ops(i);

        if (returned_ops != 0)
        {
            break;
        }
    }

    if(i >= _total_channels)
    {
        return false;
    }
    else
    {
        return true;
    }

}

SelectionKey*  PollSelector::first()
{
    if(_selected_channel_num <= 0)
    {
        return NULL;
    }

    _iterator_pos = 0;

    return  next_selected(_iterator_pos);
}


SelectionKey*  PollSelector::next()
{
    if(_selected_channel_num <= 0)
    {
        return NULL;
    }

    return  next_selected(_iterator_pos);
}

SelectionKey* PollSelector::next_selected(int& index)
{
    int i = 0;

    for ( i = index + 1; i < _total_channels; i++)
    {
        int returned_ops = _poll_wrapper.get_revent_ops(i);

        if (returned_ops != 0)
        {
            SelectionKey* sk = _channel_array[i];
            _poll_wrapper.put_revent_ops(i, 0);

            sk->channel()->translate_and_set_ready_ops(returned_ops, *sk);
            if ((sk->nio_ready_ops() & sk->nio_interest_ops()) != 0)
            {
                index = i;
                return sk;
            }
        }
    }

    return NULL;
}

void PollSelector::close_interrupt()
{
    Net::close(_fd0);
    Net::close(_fd1);

    _fd0 = -1;
    _fd1 = -1;
    _poll_wrapper.release(0);
}

Selector& PollSelector::wakeup()
{
    if (!_interrupt_triggered)
    {
        _poll_wrapper.interrupt();
        _interrupt_triggered = true;
    }
    return *this;
}

int PollSelector::translate_ready_ops(int ops)
{
    int new_ops = 0;
    if ((ops & POLLIN) != 0)
        new_ops |= Selector::OP_READ ;
    if ((ops & POLLOUT) != 0)
        new_ops |= Selector::OP_WRITE;
    if ((ops & POLLERR) != 0)
        new_ops |= Selector::OP_ERR;
    if ((ops & POLLHUP) != 0)
        new_ops |= Selector::OP_HUP;
    if ((ops & POLLNVAL) != 0)
        new_ops |= Selector::OP_NVAL;
    return new_ops;
}

int PollSelector::translate_interest_ops(int ops)
{
    int new_ops = 0;
    if ((ops & Selector::OP_READ) != 0)
        new_ops |= POLLIN;
    if ((ops & Selector::OP_WRITE) != 0)
        new_ops |= POLLOUT;
    if ((ops & Selector::OP_NVAL) != 0)
        new_ops |= POLLNVAL;
    if ((ops & Selector::OP_ERR) != 0)
        new_ops |= POLLERR;
    if ((ops & Selector::OP_HUP) != 0)
        new_ops |= POLLHUP;
    return new_ops;
}

