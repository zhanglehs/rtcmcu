#include "timer.hpp"


TimerContainer::TimerContainer(size_t capacity)
        :_capacity(capacity),
        _timer_pool(_capacity),        
        _timer_count(0)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    _base_time = tv.tv_sec;
}

TimerContainer::TimerContainer(time_t base_time, size_t capacity)
        :_capacity(capacity),
        _base_time(base_time),
        _timer_pool(_capacity),
        _timer_count(0)
{
}

TimerContainer::~TimerContainer()
{

}

int TimerContainer::init()
{
    int rv = 0;
    int j;
    for (j = 0; j < TVN_SIZE; j++) 
    {
        INIT_LIST_HEAD(_tvec_base.tv5.vec + j);
        INIT_LIST_HEAD(_tvec_base.tv4.vec + j);
        INIT_LIST_HEAD(_tvec_base.tv3.vec + j);
        INIT_LIST_HEAD(_tvec_base.tv2.vec + j);
    }
    
    for (j = 0; j < TVR_SIZE; j++)
    {
        INIT_LIST_HEAD(_tvec_base.tv1.vec + j);
    }

    rv = _timer_pool.init();
    if(rv < 0)
    {
        return -1;
    }
    _tvec_base.timer_jiffies = 0;

    return 0;
}



long TimerContainer::schedule_i(Timer* timer)
{
    unsigned long expires = timer->_expires + _tvec_base.timer_jiffies;
    unsigned long idx = timer->_expires;
    struct list_head *vec;

    if (idx < TVR_SIZE) 
    {
        int i = expires & TVR_MASK;
        vec = _tvec_base.tv1.vec + i;
    } 
    else if (idx < 1 << (TVR_BITS + TVN_BITS)) 
    {
        int i = (expires >> TVR_BITS) & TVN_MASK;
        vec = _tvec_base.tv2.vec + i;
    } 
    else if (idx < 1 << (TVR_BITS + 2 * TVN_BITS)) 
    {
        int i = (expires >> (TVR_BITS + TVN_BITS)) & TVN_MASK;
        vec = _tvec_base.tv3.vec + i;
    } 
    else if (idx < 1 << (TVR_BITS + 3 * TVN_BITS)) 
    {
        int i = (expires >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK;
        vec = _tvec_base.tv4.vec + i;
    } else if ((signed long) idx < 0) {
        /*
         * Can happen if you add a timer with expires == jiffies,
         * or you set a timer to go off in the past
         */
        vec = _tvec_base.tv1.vec + (_tvec_base.timer_jiffies & TVR_MASK);
    } 
    else 
    {
        int i;
        /* If the timeout is larger than 0xffffffff on 64-bit
         * architectures then we use the maximum timeout:
         */
        if (idx > 0xffffffffUL) {
            idx = 0xffffffffUL;
            expires = idx + _tvec_base.timer_jiffies;
        }
        i = (expires >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK;
        vec = _tvec_base.tv5.vec + i;
    }
    /*
     * Timers are FIFO:
     */
    list_add_tail(&timer->entry, vec);

    return timer->get_timer_id();
}


long TimerContainer::schedule (const TimerEventHandler &handler,
                            const time_t &future_time,
                            const time_t &interval,
                            void* param)
{
    Timer* timer_to_sched = _timer_pool.construct();

    if(timer_to_sched == NULL)
    {
        return -1;
    }
    timer_to_sched->set(_timer_pool.index(timer_to_sched), handler, param, future_time, interval);
    check_timer(timer_to_sched);
    schedule_i(timer_to_sched);
    unsigned long timer_id = timer_to_sched->get_timer_id();
    _id_timer.insert(IDTimerMap::value_type(timer_id, timer_to_sched));

    _timer_count++;
    return timer_id;
}


int TimerContainer::cancel (long timer_id,
                         int dont_call_handle_close)
{
    IDTimerMap::iterator iter = _id_timer.find(timer_id);
    if(iter == _id_timer.end())
    {
        return -1;
    }

    Timer* cancel_timer = iter->second;
    cancel_timer->cancel();
    list_del(&cancel_timer->entry);

    _id_timer.erase(iter);
    _cancel_timer_list.push_back(cancel_timer);          
    _timer_count--;

    return 0;
}

int TimerContainer::expire (const time_t &jiffies_inner)
{
    Timer* expired_timer;

    while (jiffies_inner >=  _tvec_base.timer_jiffies)
    {
        struct list_head work_list = LIST_HEAD_INIT(work_list);
        struct list_head *head = &work_list;
        int index = _tvec_base.timer_jiffies & TVR_MASK;

        /*
         * Cascade timers:
         */
        if (!index &&
                (!cascade(&_tvec_base.tv2, INDEX(0))) &&
                (!cascade(&_tvec_base.tv3, INDEX(1))) &&
                !cascade(&_tvec_base.tv4, INDEX(2)))
        {
            cascade(&_tvec_base.tv5, INDEX(3));
        }


        list_splice_init(_tvec_base.tv1.vec + index, &work_list);

        while(!list_empty(head))
        {
            expired_timer = list_entry(head->next,Timer,entry);
            //time out event here
            expired_timer->timeout();

            time_t interval = expired_timer->get_interval();
            if(expired_timer->canceled())
            {
                continue;
            }
            
            if(interval != 0)
            {
                list_del(&expired_timer->entry);
                expired_timer->set_expires(expired_timer->get_interval());
                schedule_i(expired_timer);
            }
            else
            {
                cancel(expired_timer->get_timer_id());
            }
        }

        ++_tvec_base.timer_jiffies;
    }

    return 0;
}




int TimerContainer::cascade(tvec_t *tv, int index)
{
    /* cascade all the timers from tv up one level */
    struct list_head *head, *curr;

    head = tv->vec + index;
    curr = head->next;
    /*
     * We are removing _all_ timers from the list, so we don't  have to
     * detach them individually, just clear the list afterwards.
     */
    while (curr != head) 
    {
        Timer *tmp;

        tmp = list_entry(curr, Timer, entry);
        curr = curr->next;
        schedule_i(tmp);
    }
    INIT_LIST_HEAD(head);

    return index;
}


