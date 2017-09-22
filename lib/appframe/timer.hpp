/**
 * \file
 * \brief ¶¨Ê±Æ÷
 */
#ifndef _TIMER_HPP_
#define _TIMER_HPP_

#include <stdint.h>
#include <sys/time.h>
#include <list>
#include <ext/hash_map>
//debug
#include <stdio.h>
//end
#include "array_object_pool.hpp"
#include "list.hpp"
#include "timer_event_handler.hpp"

using namespace std;
using namespace __gnu_cxx;

class TimerContainer
{
private:
    enum
    {
        TVN_BITS = 6,
        TVR_BITS  = 8,
        TVN_SIZE  = (1 << TVN_BITS),
        TVR_SIZE  = (1 << TVR_BITS),
        TVN_MASK  = (TVN_SIZE - 1),
        TVR_MASK = (TVR_SIZE - 1),
    };

    class Timer
    {
    public:
        /// Default constructor
        Timer(void)
                :_id(0),
                _handler(NULL),
                _expires(0),
                _interval(0),
                _canceled(false),                
                _param(NULL)
        {}

        Timer (unsigned long timer_id,
               const TimerEventHandler &handler,
               void*param,
               const time_t &expires,
               const time_t &interval)
                :_id( timer_id),
                _handler((TimerEventHandler*)&handler),
                _expires(expires),
                _interval(interval),
                _canceled(false),
                _param(param)
        {
        }
        /// Destructor
        ~Timer(void){}

        void set (unsigned long timer_id,
                      const TimerEventHandler &handler,
                      void*param,
                      const time_t &expires,
                      const time_t &interval)
        {
            _id         = timer_id;
            _handler = (TimerEventHandler*)&handler;
            _expires = expires;
            _interval = interval;
            _param   =param;
        }

        void timeout()
        {
            if(_handler)
            {
                _handler->handle_timeout(_param);
            }
        }
        TimerEventHandler& get_handler (void)
        {
            return * _handler;
        }

        void set_handler (TimerEventHandler &handler)
        {
            _handler= &handler;
        }


        /// Get the timer value.
        const time_t &get_expires(void) const
        {
            return _expires;
        }

        /// Set the timer value.
        void set_expires (const time_t &timer_value)
        {
            _expires = timer_value;
        }

        /// Get the timer interval.
        const time_t &get_interval (void) const
        {
            return _interval;
        }

        /// Set the timer interval.
        void set_interval (const time_t &interval)
        {
            _interval = interval;
        }

        bool canceled()
        {
            return _canceled;
        }

        bool cancel()
        {
            return _canceled = true;
        }
                
        /// Get the timer_id.
        unsigned long get_timer_id (void) const
        {
            return _id;
        }

        /// Set the timer_id.
        void set_timer_id (unsigned long timer_id)
        {
            _id = timer_id;
        }

        unsigned long   _id;

        TimerEventHandler*  _handler;

        /// Time until the timer expires.
        time_t _expires;

        /// If this is a periodic timer this holds the time until the next
        /// timeout.
        time_t _interval;

        list_head entry;

        bool      _canceled;
        //
        void * _param;
    };

    struct tvec_t
    {
        struct list_head vec[TVN_SIZE];
    };

    struct tvec_root_t
    {
        struct list_head vec[TVR_SIZE];
    } ;

    struct tvec_base_t
    {
        time_t  timer_jiffies;
        tvec_root_t tv1;
        tvec_t tv2;
        tvec_t tv3;
        tvec_t tv4;
        tvec_t tv5;
    } ;

    typedef hash_map<long, Timer*>  IDTimerMap;
    typedef ArrayObjectPool<Timer>    TimerPool;

public:
    TimerContainer(size_t capacity);
    TimerContainer(time_t base_time, size_t capacity);
    ~TimerContainer();

    int init();
    /// True if queue is empty, else false.
    uint32_t is_empty (void);

    long schedule (const TimerEventHandler &handler,
                   const time_t &future_time,
                   const time_t &interval = 0,
                   void* param = NULL);

    int reset_interval (long timer_id,
                        const time_t &interval) ;
    int cancel (long timer_id,
                int dont_call_handle_close = 1) ;
    int expire (const time_t &current_time);

    void poll();
private:
    void check_timer(Timer* timer);
    unsigned long next_timer_id();
    long schedule_i (Timer* timer);
    int cascade(tvec_t *tv, int index);
    unsigned long INDEX(int N);

private:
    size_t                _capacity;
    time_t      	        _base_time;
    tvec_base_t        _tvec_base;
    IDTimerMap        _id_timer;
    TimerPool           _timer_pool;
    uint32_t              _timer_count;
    list<Timer*>           _cancel_timer_list;
};


inline
unsigned long TimerContainer::INDEX(int N)
{
    return	(_tvec_base.timer_jiffies >> (TVR_BITS + N * TVN_BITS)) & TVN_MASK;
}

inline
uint32_t TimerContainer::is_empty (void)
{
    return _timer_count;
}

inline
void TimerContainer::check_timer(Timer* timer)
{
}

inline
void TimerContainer::poll()
{  
    list<Timer*>::iterator iter_cancel = _cancel_timer_list.begin();

    for(;iter_cancel != _cancel_timer_list.end(); iter_cancel++)
    {
        _timer_pool.destroy(*iter_cancel);
    }

    _cancel_timer_list.clear();

    struct timeval tv;
    gettimeofday(&tv,NULL);
    time_t jiffies_inner = tv.tv_sec - _base_time;

    //in kernel 2.6,gettimeofday() or time() may jump to future about 4398 seconds
    //to avoid this bug, we add the judgement below.
    //08.4.21
    if(jiffies_inner > 3600 + _tvec_base.timer_jiffies)
    {
        //printf("abnormal timer, jiffies_inner : %ld _tv.timer_jiffies : %ld now : %ld, base : %ld\n ",
        //    jiffies_inner,
        //    _tvec_base.timer_jiffies,
        //    tv.tv_sec,
        //    _base_time);
        return;
    }
    
    if(jiffies_inner >=  _tvec_base.timer_jiffies)
    { 
        expire(jiffies_inner);
    }  
}

#endif
