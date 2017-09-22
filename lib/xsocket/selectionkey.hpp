#ifndef _BASE_NET_SELECTIONKEY_H_
#define _BASE_NET_SELECTIONKEY_H_
#include <set>
#include "selectablechannel.hpp"
using namespace std;


class Selector;
class Attachment;

class SelectionKey
{
    friend class Selector;
    friend class SelectableChannel;
private:
    volatile bool             _valid;

    SelectableChannel*  _channel;
    Selector*                 _selector;
    Attachment*               _attachment;

    // Index for a pollfd array in Selector that this key is registered with
    int                           _index;

    volatile int               _interest_ops;
    int                          _ready_ops;

public:
    enum
    {
        OP_READ = 1 << 0,
        OP_WRITE = 1 << 2,
        OP_CONNECT = 1 << 3,
        OP_ACCEPT = 1 << 4,
    };

    SelectionKey();
    SelectionKey(SelectableChannel& ch, Selector& sel) ;

    SelectableChannel* channel();

    Selector* selector()
    {
        return _selector;
    }

    int get_index()
    {
        return _index;
    }

    void set_index(int i)
    {
        _index = i;
    }

    bool is_valid()
    {
        //return _valid;
        return false;
    }

    int interest_ops()
    {
        ensure_valid();
        return _interest_ops;
    }

    SelectionKey& interest_ops(int ops) ;

    int ready_ops()
    {
        ensure_valid();
        return _ready_ops;
    }

    // The nio versions of these operations do not care if a key
    // has been invalidated. They are for internal use by nio code.

    void ensure_valid();

    void nio_ready_ops(int ops)
    {
        _ready_ops = ops;
    }

    int nio_ready_ops()
    {
        return _ready_ops;
    }

    SelectionKey& nio_interest_ops(int ops);

    int nio_interest_ops()
    {
        return _interest_ops;
    }

public:
    bool is_readable()
    {
        return (ready_ops() & OP_READ) != 0;
    }

    bool is_writable()
    {
        return (ready_ops() & OP_WRITE) != 0;
    }

    bool is_connectable()
    {
        return (ready_ops() & OP_CONNECT) != 0;
    }

    bool is_acceptable()
    {
        return (ready_ops() & OP_ACCEPT) != 0;
    }

public:
    Attachment* attach(Attachment* ob);

    Attachment* attachment()
    {
        return _attachment;
    }
};


typedef   set<SelectionKey*> SelectionKeySet;
#endif

