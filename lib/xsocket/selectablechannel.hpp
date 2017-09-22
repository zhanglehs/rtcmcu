#ifndef _BASE_NET_SELECTABLECHANNEL_H_
#define _BASE_NET_SELECTABLECHANNEL_H_

#include <vector>
#include <set>

using namespace std;

class SelectorProvider;
class Attachment;
class Selector;
class SelectionKey;

class SelectableChannel
{
    friend class Selector;
    friend class SelectionKey;
private:
    SelectorProvider* _provider;

    SelectionKey*       _key;                  // = null;

    bool                   _registered;
    bool                   _blocking;          // = true;
    volatile bool        _open;                // = true;


public:
    SelectableChannel(SelectorProvider* provider);
    SelectableChannel();
    
    virtual ~SelectableChannel();

    SelectionKey*  key();
    void remove_key(SelectionKey& key);
    SelectionKey* enregister(Selector& sel, int ops, Attachment* att);
    SelectionKey* enregister(Selector& sel, int ops);
    int deregister();

    SelectableChannel& configure_blocking(bool block);

    bool is_registered();
    bool is_blocking();
    SelectorProvider* provider();
    bool is_open();
    virtual void close();

    virtual int get_fd()= 0;

    virtual int valid_ops()
    {
        return 0;
    }

    virtual bool translate_and_update_ready_ops(int ops, SelectionKey& sk){return true;}
    virtual bool translate_and_set_ready_ops(int ops, SelectionKey& sk){return true;}

    virtual bool translate_and_set_interest_ops(int ops, SelectionKey& sk){return true;}
    virtual void kill(){return ;}

protected:
    virtual void impl_configure_blocking(bool block){return ;}



};


inline
SelectorProvider* SelectableChannel::provider()
{
    return _provider;
}

inline
bool SelectableChannel::is_registered()
{
    return _registered;
}

inline
bool SelectableChannel::is_blocking()
{
    return _blocking;
}

inline
bool SelectableChannel::is_open()
{
    return _open;
}

inline
SelectionKey*  SelectableChannel::key()
{
    return _key;
}

inline
void SelectableChannel::remove_key(SelectionKey& key)
{
    if((&key) == _key)
    {
        _key = NULL;
        _registered = false;
    }
}
#endif

