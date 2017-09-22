#include "event_pump_libevent.h"


namespace base {

    EventPumpLibevent *EventPumpLibevent::_event_pump = NULL;

    EventPumpLibevent *EventPumpLibevent::get_inst()
    {
        if (NULL == _event_pump)
            _event_pump = new EventPumpLibevent();
        return _event_pump;
    }

    void EventPumpLibevent::init(event_base* event_base)
    {
        _event_base = event_base;
    }

    EventPumpLibevent::EventPumpLibevent()
    {

    }

    EventPumpLibevent::~EventPumpLibevent()
    {

    }

    bool EventPumpLibevent::watch_file_descriptor(int fd,
                                                  event *ev,
                                                  bool persistent,
                                                  int mode,
                                                  Watcher *delegate) 
    {
        int event_mask = persistent ? EV_PERSIST : 0;
        if (mode & WATCH_READ) {
            event_mask |= EV_READ;
        }
        if (mode & WATCH_WRITE) {
            event_mask |= EV_WRITE;
        }

        levent_set(ev, fd, event_mask, on_libevent_notification, delegate);
        event_base_set(_event_base, ev);
        levent_add(ev, 0);
        return true;
    }

    bool EventPumpLibevent::watch_timer(event *ev_timer, TimerWatcher *delegate)
    {
        return true;
    }

    void EventPumpLibevent::disable_write(int fd, event *ev, Watcher *delegate)
    {
        if (event_pending(ev, EV_WRITE, NULL))
        {
            levent_del(ev);
            levent_set(ev, fd, EV_READ | EV_PERSIST, on_libevent_notification, delegate);
            event_base_set(_event_base, ev);
            levent_add(ev, 0);
        }
    }

    void EventPumpLibevent::enable_write(int fd, event *ev, Watcher *delegate)
    {
        if (event_pending(ev, EV_WRITE, NULL))
            return;
        levent_del(ev);
        levent_set(ev, fd, EV_READ | EV_WRITE | EV_PERSIST, on_libevent_notification, delegate);
        event_base_set(_event_base, ev);
        levent_add(ev, 0);
    }

    // static
    void EventPumpLibevent::on_libevent_notification(int fd,
                                                     short flags,
                                                     void* context) 
    {
        Watcher* watcher = static_cast<Watcher*>(context);

        if ((flags & (EV_READ | EV_WRITE)) == (EV_READ | EV_WRITE)) {
            watcher->OnFileCanWriteWithoutBlocking(fd);
            watcher->OnFileCanReadWithoutBlocking(fd);
        }
        else if (flags & EV_WRITE) {
            watcher->OnFileCanWriteWithoutBlocking(fd);
        }
        else if (flags & EV_READ) {
            watcher->OnFileCanReadWithoutBlocking(fd);
        }
    }

}
