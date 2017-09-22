#ifndef _BASE_NET_POLLSELECTOR_H_
#define _BASE_NET_POLLSELECTOR_H_

#include <xthread/runnable.hpp>

#include "selector.hpp"
#include "selectionkey.hpp"
#include "selectablechannel.hpp"
#include "pollarraywrapper.hpp"

class SelectorProvider;

class PollSelector : public Selector
{
private:
    bool _selector_open;

    int   _capacity;
    // The poll fd array
    PollArrayWrapper _poll_wrapper;
    
    // The list of SelectableChannels serviced by this Selector
    SelectionKeyPool _selection_key_pool;
    SelectionKey**  _channel_array;
    int                   _total_channels;
    int                   _channel_offset;
    int                   _selected_channel_num;
    
    // File descriptors used for interrupt
    int _fd0;
    int _fd1;

    bool _interrupt_triggered;

    int   _iterator_pos;
public:

    PollSelector();
    virtual ~PollSelector();

    int   init(int max_channels);
    void close();
    bool is_open();

    int select();
    int select(long timeout);
    int select_now();

    Selector& wakeup();
    void cancel(SelectionKey& k);

    bool has_next();
    SelectionKey* first();
    SelectionKey* next();

    SelectionKey* enregister(SelectableChannel& ch, int ops, Attachment* attachment);
    void deregister(SelectionKey& key);

    void put_event_ops(SelectionKey& sk, int ops) ;
private:
    int    do_select(long timeout);
    void close_interrupt();
    SelectionKey*   next_selected(int& index);
    int translate_ready_ops(int ops);
    int translate_interest_ops(int ops);

};

inline
bool PollSelector::is_open()
{
    return _selector_open;
}

inline
int PollSelector::select()
{
    return select(0);
}

inline
int PollSelector::select(long timeout)
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

inline
int PollSelector::select_now()
{
    if (!is_open())
    {
        return -1;
    }

    return do_select(0);
}

#endif

