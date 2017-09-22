#ifndef _BASE_NET_SELECTOR_H_
#define _BASE_NET_SELECTOR_H_

#include "appframe/array_object_pool.hpp"
#include "selectionkey.hpp"

class SelectionKey;
class SelectableChannel;
class Attachment;
class SelectorProvider;


typedef ArrayObjectPool<SelectionKey> SelectionKeyPool;

class Selector
{
    friend class SelectableChannel;
    friend class SelectionKey;

public:
    enum
    {
        OP_READ = 0x001,
        OP_WRITE= 0x004,
        OP_ERR= 0x008,
        OP_HUP= 0x010,
        OP_ET= (1 << 31),
        OP_NVAL= 0x020,
    };

    Selector()
    {
    }
    virtual ~Selector()
    {
    };

    virtual int   init(int max_channels)=0;
    virtual void close() = 0;
    virtual bool is_open()= 0;


    virtual int select()= 0;
    virtual int select(long timeout)= 0;

    virtual bool has_next() = 0;
    virtual SelectionKey* first() = 0;
    virtual SelectionKey* next() = 0;
    virtual int select_now()= 0;

    virtual Selector& wakeup()= 0;
    virtual void put_event_ops(SelectionKey& sk, int ops) = 0;

protected:
    virtual SelectionKey* enregister(SelectableChannel& ch, int ops, Attachment* attachment)= 0;
    virtual   void deregister(SelectionKey& key) = 0;
    virtual void cancel(SelectionKey& k)= 0;
};

#endif

