#ifndef _LCDN_NET_CHANNEL_H_
#define _LCDN_NET_CHANNEL_H_

#include "event.h"
#include "event_loop.h"

namespace lcdn
{
namespace net
{

class Channel
{
public:
    Channel(int fd);
    ~Channel();
    void set_event_loop(EventLoop *loop);
    void set_watcher(EventLoop::Watcher *watcher);
    void enable_read();
    void enable_write();
    void disable_write();
    void enable_all();
    void disable_all();
    bool is_writing() const;
    EventLoop *event_loop() const;
    int fd() const;
    struct event *event() const;
    void set_event(struct event*);
private:
    void _update();
private:
    struct event* _event;
    int _event_mask;
    int _fd;
    EventLoop* _loop;
    EventLoop::Watcher* _watcher;
};

} // namespace net
} // namespace lcdn

#endif // _LCDN_NET_CHANNEL_H_

