#ifndef _LCDN_NET_EVENT_LOOP_H_
#define _LCDN_NET_EVENT_LOOP_H_

#include <deque>
#include <event.h>

namespace lcdn
{
namespace net
{

class Channel;

class EventLoop
{
public:
    class Watcher
    {
    public:
        virtual void on_read() = 0;
        virtual void on_write() = 0;
        virtual ~Watcher() {}
    };

    class TimerWatcher
    {
        friend class EventLoop;

    public:
        TimerWatcher();
        virtual ~TimerWatcher() { _reset(); }
        virtual void on_timer() = 0;
    private:
        void _reset();
        bool _start_rounds(EventLoop* loop, int64_t interval, uint64_t round);
        bool _re_rounds();
        bool _start_once(EventLoop* loop, int64_t interval);
        bool _start_forever(EventLoop* loop, int64_t interval);
        static void _on_timer_notification(int fd, short flags, void *context);
    private:
        struct event* _event;
        int64_t _interval;
        uint64_t _round;
        uint64_t _cur_round;
        EventLoop* _loop;
    };

    class Task
    {
    public:
        virtual ~Task() {}
        // Tasks are automatically deleted after Run is called.
        virtual void run() = 0;
    };

    enum Mode
    {
        WATCH_READ = 1 << 0,
        WATCH_WRITE = 1 << 1,
        WATCH_READ_WRITE = WATCH_READ | WATCH_WRITE
    };

public:
    EventLoop(event_base *event_base);
    ~EventLoop();

    int loop();
    int quit();

    // timer
    void run_once(int64_t interval, TimerWatcher* delegate);
    void run_forever(int64_t interval, TimerWatcher* delegate);

    // task
    void post_task(Task* task);

    int watch_file_descriptor(int fd, Channel* channel, bool persistent, int mode, Watcher *delegate);
private:
    class TaskRuner : public EventLoop::TimerWatcher
    {
    public:
        TaskRuner(EventLoop * loop) : _loop(loop) {}
        virtual ~TaskRuner() {}
        virtual void on_timer();
    private:
        EventLoop* _loop;
    };
    
    void _run_tasks();

    static void on_libevent_notification(int fd, short flags, void *context);
private:
    event_base* _event_base;
    std::deque<Task*> _pending_tasks_queue;
    TaskRuner _task_runner;
};

} // namespace net
} // namespace lcdn

#endif // _LCDN_NET_EVENT_LOOP_H_
