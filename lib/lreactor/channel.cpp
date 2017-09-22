#include "channel.h"
#include <stddef.h>
 
using namespace lcdn;
using namespace lcdn::net;

Channel::Channel(int fd) :
    _event(NULL),
    _event_mask(0),
    _fd(fd),
    _loop(NULL),
    _watcher(NULL) 
{
}

Channel::~Channel()
{
    if (_event)
    {
        disable_all();
        delete _event;
        _event = NULL;
    }
    _watcher = NULL;
    _loop = NULL;
    _fd = -1;
}

void Channel::set_event_loop(EventLoop *loop)
{
    _loop = loop;
}

void Channel::set_watcher(EventLoop::Watcher *watcher)
{
    _watcher = watcher;
}

void Channel::enable_read()
{ 
    _event_mask |= EventLoop::WATCH_READ;
    _update();
}

void Channel::enable_write()
{ 
    _event_mask |= EventLoop::WATCH_WRITE;
    _update(); 
}

void Channel::disable_write() 
{ 
    _event_mask &= ~EventLoop::WATCH_WRITE;
    _update(); 
}

void Channel::enable_all() 
{ 
    _event_mask |= EventLoop::WATCH_READ;
    _event_mask |= EventLoop::WATCH_WRITE;
    _update(); 
}

void Channel::disable_all() 
{ 
    if (_event_mask && _event)
    {
        _event_mask = 0;
        event_del(_event);
    }
}

bool Channel::is_writing() const 
{ 
    if (event_pending(_event, EV_WRITE, NULL))
        return true;
    return false;
}

EventLoop *Channel::event_loop() const 
{ 
    return _loop; 
}

int Channel::fd() const 
{ 
    return _fd;
}

struct event *Channel::event() const 
{ 
    return _event; 
}

void Channel::set_event(struct event* event)
{
    _event = event;
}

void Channel::_update()
{
    _loop->watch_file_descriptor(_fd, this, true, _event_mask, _watcher);
}

