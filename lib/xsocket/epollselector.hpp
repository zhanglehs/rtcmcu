#ifndef _BASE_NET_EPOLLSELECTOR_H_
#define _BASE_NET_EPOLLSELECTOR_H_


#include <google/dense_hash_map>
#include <ext/hash_map>
#include "selector.hpp"
#include <xthread/runnable.hpp>
#include "selectionkey.hpp"
#include "selectablechannel.hpp"
#include "epollwrapper.hpp"
#include "net.hpp"

using namespace google;
using namespace std;
using namespace __gnu_cxx;

class SelectorProvider;

class EPollSelector : public Selector
{
private:
    int _capacity;
    // The poll fd array
    EPollArrayWrapper _epoll_wrapper;
    // The list of SelectableChannels serviced by this Selector
    SelectionKeyPool _selection_key_pool;    
    hash_map<int, SelectionKey* > _channel_array;

    // The number of valid channels in this Selector's poll array
    int _selected_channel_num;

    // File descriptors used for interrupt
    int _fd0;
    int _fd1;

    // Lock for interrupt triggering and clearing
    bool _interrupt_triggered;
    bool _selector_open;

    int   _iterator_pos;
public:

    EPollSelector();
    virtual ~EPollSelector();

    int  init(int max_channels);
    void close();
    bool is_open();

    int select();
    int select(long timeout);
    int select_now();

    bool has_next();
    SelectionKey* first();
    SelectionKey* next();

    Selector& wakeup();
    void cancel(SelectionKey& k);
    void put_event_ops(SelectionKey& sk, int ops) ;

protected:
    SelectionKey* enregister(SelectableChannel& ch, int ops, Attachment* attachment);
    void deregister(SelectionKey& key);

private:
    int do_select(long timeout);
    void close_interrupt();
    void proc_interrupt();
    SelectionKey*  selected_key(int pos );
    int translate_ready_ops(int oldOps);
    int translate_interest_ops(int ops);

};

inline
bool EPollSelector::is_open()
{
    return _selector_open;
}

inline
SelectionKey*  EPollSelector::selected_key(int pos)
{
    epoll_event* event = _epoll_wrapper.get_revent_ops(pos);


    SelectionKey* sk = _channel_array[event->data.fd];

    if(sk == NULL)
    {
        return NULL;
    }
    int returned_ops = event->events;

    sk->channel()->translate_and_set_ready_ops(returned_ops, *sk);
    if ((sk->nio_ready_ops() & sk->nio_interest_ops()) != 0)
    {
        return sk;
    }
    else
    {
        return NULL;
    }
}

inline
void EPollSelector::proc_interrupt()
{
    epoll_event* event = _epoll_wrapper.get_revent_ops(_iterator_pos);

    if (event->data.fd == _fd0)
    {
        // Clear the wakeup pipe
        Net::drain(_fd0);
        _interrupt_triggered = false;
        _iterator_pos++;
    }

}

#endif

