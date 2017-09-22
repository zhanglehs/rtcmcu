#ifndef _EVENT_PUMP_LIBEVENT_
#define _EVENT_PUMP_LIBEVENT_

#include "util/levent.h"

namespace base {

    class EventPumpLibevent {
    public:
        // Used with WatchFileDescriptor to asynchronously monitor the I/O readiness
        // of a file descriptor.
        class Watcher {
        public:
            // Called from Run when an FD can be read from/written to
            // without blocking
            virtual void OnFileCanReadWithoutBlocking(int fd) = 0;
            virtual void OnFileCanWriteWithoutBlocking(int fd) = 0;
        protected:
            virtual ~Watcher() {}
        };

        class TimerWatcher {
        public:
            virtual void OnTimerExpired() = 0;
        protected:
            virtual ~TimerWatcher() {}
        };

        enum Mode {
            WATCH_READ = 1 << 0,
            WATCH_WRITE = 1 << 1,
            WATCH_READ_WRITE = WATCH_READ | WATCH_WRITE
        };

        static EventPumpLibevent *get_inst();
        void init(event_base* event_base);
        bool watch_file_descriptor(int fd,
                                 event *ev,
                                 bool persistent,
                                 int mode,
                                 Watcher *delegate);
        bool watch_timer(event *ev_timer, TimerWatcher *delegate);
        void disable_write(int fd, event *ev, Watcher *delegate);
        void enable_write(int fd, event *ev, Watcher *delegate);
    private:
        EventPumpLibevent();
        ~EventPumpLibevent();
        static void on_libevent_notification(int fd, short flags,
                                           void* context);
    private:
        event_base* _event_base;
        static EventPumpLibevent* _event_pump;
    };
}

#endif // _EVENT_PUMP_LIBEVENT_
