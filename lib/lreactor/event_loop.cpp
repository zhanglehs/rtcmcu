#include "event_loop.h"
#include "channel.h"

#include <stddef.h>
#include <event.h>

using namespace std;
using namespace lcdn;
using namespace lcdn::net;

EventLoop::EventLoop(event_base *event_base)
    : _event_base(event_base),
      _task_runner(this)
{
}

EventLoop::~EventLoop()
{
}

int EventLoop::loop()
{
    // start task runner
    run_forever(10, &_task_runner);

    event_base_dispatch(_event_base);
    return 0;
}

int EventLoop::quit()
{
    event_loopbreak();
    return 0;
}

void EventLoop::run_once(int64_t interval, EventLoop::TimerWatcher* delegate)
{
    delegate->_start_once(this, interval);
}

void EventLoop::run_forever(int64_t interval, EventLoop::TimerWatcher* delegate)
{
    delegate->_start_forever(this, interval);
}

void EventLoop::post_task(Task* task)
{
    _pending_tasks_queue.push_back(task);
}

int EventLoop::watch_file_descriptor(int fd, Channel *channel, bool persistent, int mode, Watcher *delegate)
{
    int event_mask = persistent ? EV_PERSIST : 0;
    if (mode & WATCH_READ) {
        event_mask |= EV_READ;
    }
    if (mode & WATCH_WRITE) {
        event_mask |= EV_WRITE;
    }

    struct event* ev = channel->event();
    if (NULL == ev) {
        ev = new struct event;
        channel->set_event(ev);
    }
    else
        event_del(ev);

    event_set(ev, fd, event_mask, on_libevent_notification, delegate);

    // Tell libevent which event loop this socket will belong to when we add it.
    if (event_base_set(_event_base, ev))
        return -1;

    // Add this socket to the list of monitored sockets.
    if (event_add(ev, 0))
        return -1;

    return 0;
}

void EventLoop::TaskRuner::on_timer()
{
    _loop->_run_tasks();
}

void EventLoop::_run_tasks()
{
    while (!_pending_tasks_queue.empty())
    {
        Task* task = _pending_tasks_queue.front();
        _pending_tasks_queue.pop_front();
        task->run();
        delete task;
    }
}

// static
void EventLoop::on_libevent_notification(int fd, short flags, void *context)
{
    if (NULL == context) return;

    Watcher *watcher = static_cast<Watcher*>(context);
    if (flags & (EV_READ | EV_WRITE) == EV_READ | EV_WRITE)
    {
        watcher->on_read();
        watcher->on_write();
    }
    else if (flags & EV_READ == EV_READ)
    {
        watcher->on_read();
    }
    else if (flags & EV_WRITE == EV_WRITE)
    {
        watcher->on_write();
    }
}

EventLoop::TimerWatcher::TimerWatcher()
    : _event(NULL),
      _interval(0),
      _round(1),
      _cur_round(0),
      _loop(NULL)
{
}

void EventLoop::TimerWatcher::_reset()
{
    if (NULL != _event)
    {
        delete _event;
        _event = NULL;
    }
    _loop = NULL;
}

bool EventLoop::TimerWatcher::_start_rounds(EventLoop* loop, int64_t interval, uint64_t round)
{
    if (NULL == _event)
    {
        _event = new (struct event);
    }
    
    if (NULL == _event)
    {
        return false;
    }

    event_set(_event, -1, (round > 1) ? EV_PERSIST : 0, _on_timer_notification, this);
    if (0 != event_base_set(loop->_event_base, _event))
    {
        return false;
    }

    struct timeval tv = {};
    evutil_timerclear(&tv);
    tv.tv_sec = interval / 1000;
    tv.tv_usec = (interval % 1000) * 1000;
    if (0 != event_add(_event, &tv))
    {
        _reset();
        return false;
    }

    _interval = interval;
    _round = round;
    _cur_round = 0;
    _loop = loop;
    return true;
}

bool EventLoop::TimerWatcher::_re_rounds()
{
    event_set(_event, -1, (_round > 1) ? EV_PERSIST : 0, _on_timer_notification, this);
    if (0 != event_base_set(_loop->_event_base, _event))
    {
        return false;
    }

    struct timeval tv = {};
    evutil_timerclear(&tv);
    tv.tv_sec = _interval / 1000;
    tv.tv_usec = (_interval % 1000) * 1000;
    if (0 != event_add(_event, &tv))
    {
        _reset();
        return false;
    }
}

bool EventLoop::TimerWatcher::_start_once(EventLoop* loop, int64_t interval)
{
    _start_rounds(loop, interval, 1);
}

bool EventLoop::TimerWatcher::_start_forever(EventLoop* loop, int64_t interval)
{
    _start_rounds(loop, interval, -1);
}

// static
void EventLoop::TimerWatcher::_on_timer_notification(int fd, short flags, void *context)
{
    if (NULL == context) return;

    TimerWatcher *watcher = static_cast<TimerWatcher*>(context);

    watcher->_cur_round++;
    if (watcher->_cur_round >= watcher->_round)
    {
        event_del(watcher->_event);
        watcher->_reset();
    }

    watcher->on_timer();
    watcher->_re_rounds();
}

