#ifndef _THREAD_POOL_EXECUTOR_H_
#define _THREAD_POOL_EXECUTOR_H_

#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <ext/hash_set>
#include "thread.hpp"
#include "fast_mutex.hpp"
#include "guard.hpp"
#include "condition.hpp"
#include "executor.hpp"
#include "circular_array_queue.hpp"


using namespace std;
using namespace __gnu_cxx;

template<typename Task>
class ThreadPoolExecutor;

template<typename Task>
class Worker;



template<typename Task>
class WorkerThread: public Thread
{
 public:
    WorkerThread(ThreadPoolExecutor<Task>& executor, Task& first_task)
    :_executor(executor),
     _task(first_task)
    {

    }
    
    void run();
    void run_task(Task& task);
    
private:    
    ThreadPoolExecutor<Task>& _executor;      
    Task                        _task;
};

template<typename Task>
class ThreadPoolExecutor//: public Executor<Task>
{
public:    
    friend  class WorkerThread<Task> ;
    typedef  WorkerThread<Task>                Worker;
    typedef  hash_set<Worker*>                   WorkerSet;
    typedef  CircularArrayQueue<Task>        TaskQueue;    
    typedef  CircularArrayQueue<Worker*>    UnregisterQueue;   
  
public:

    ThreadPoolExecutor(int core_pool_size, int maximum_pool_size, int keep_alive_time);
    virtual ~ThreadPoolExecutor();
    int init();
    int execute(Task& task);

protected:
    int get_task(Task& task);

    
    /**
     * Create and return a new thread running firstTask as its first
     * task. Call only while holding mainLock
     * @param firstTask the task the new thread should run first (or
     * null if none)
     * @return the new thread, or null if threadFactory fails to create thread
     */
    Thread* add_worker(Task& first_task) ;

    /**
     * Create and start a new thread running firstTask as its first
     * task, only if fewer than corePoolSize threads are running.
     * @param firstTask the task the new thread should run first (or
     * null if none)
     * @return true if successful.
     */
    bool add_if_under_maximum_pool_size(Task& first_task) ;

    void register_worker(Worker& w);
    void deregister_worker(Worker& w);

public:
     static const int TASK_QUEUE_FACTOR = 25600;
public:
    /**
     * Queue used for holding tasks and handing off to worker threads.
     */

    TaskQueue* _task_queue;

    /**
     * Set containing all worker threads in pool.
     */
    WorkerSet _workers;
    UnregisterQueue*       _to_unregister_workers;
    

    /**
     * Core pool size, updated only while holding mainLock,
     * but volatile to allow concurrent readability even
     * during updates.
     */
     volatile int   _core_pool_size;

    /**
     * Maximum pool size, updated only while holding mainLock
     * but volatile to allow concurrent readability even
     * during updates.
     */
     volatile int   _maximum_pool_size;
    
    /**
     * Timeout in nanoseconds for idle threads waiting for work.
     * Threads use this timeout only when there are more than
     * corePoolSize present. Otherwise they wait forever for new work.
     */
     volatile long  _keep_alive_time;
    /**
     * Current pool size, updated only while holding mainLock
     * but volatile to allow concurrent readability even
     * during updates.
     */
     volatile int   _pool_size;

    /**
     * Lifecycle state
     */
    volatile int _run_state;

    // Special values for runState
    /** Normal, not-shutdown mode */
    static const int RUNNING    = 0;
    /** Controlled shutdown mode */
    static const int SHUTDOWN   = 1;
    /** Immediate shutdown mode */
    static const int STOP       = 2;
    /** Final state */
    static const int TERMINATED = 3;

    /**
     * Tracks largest attained pool size.
     */
     int _largest_pool_size;

    /**
     * Counter for completed tasks. Updated only on termination of
     * worker threads.
     */
    long     _completed_task_count;

    FastMutex   _main_lock;
    
    FastMutex    _push_lock;
    FastMutex    _pop_lock;

    Condition _not_full;
    Condition _not_empty;    
};


/**
 * Create and return a new thread running firstTask as its first
 * task. Call only while holding mainLock
 * @param firstTask the task the new thread should run first (or
 * null if none)
 * @return the new thread, or null if threadFactory fails to create thread
 */

template<typename Task>
inline
void WorkerThread<Task>::run()
{
    _executor.register_worker(*this);
    try
    {
        do
        {
            run_task(_task);
        }
        while(_executor.get_task(_task) == 0);
    }
    catch(...)
    {     
        throw;
    }
    
    _executor.deregister_worker(*this);            
}

template<typename Task>
inline
void WorkerThread<Task>::run_task(Task& task) 
{
    task.run();
}

template<typename Task>
inline 
Thread* ThreadPoolExecutor<Task>::add_worker(Task& first_task) 
{
    Worker* w = new Worker(*this, first_task);
    return w;
}

template<typename Task>
inline 
void ThreadPoolExecutor<Task>::register_worker(Worker& w)
{
    Guard<FastMutex> guard(_main_lock);
    
    _workers.insert(&w); 

}

template<typename Task>
inline 
void ThreadPoolExecutor<Task>::deregister_worker(Worker& w)
{
    Guard<FastMutex> guard(_main_lock);
    _to_unregister_workers->push(&w); 

}

template<typename Task>
inline 
ThreadPoolExecutor<Task>::ThreadPoolExecutor(int core_pool_size,
                                                                        int maximum_pool_size,
                                                                        int keep_alive_time
                                                                        )
:_task_queue(NULL),
_to_unregister_workers(NULL),
_core_pool_size(core_pool_size),
_maximum_pool_size(maximum_pool_size), 
_keep_alive_time(keep_alive_time),     
_pool_size(0),
_largest_pool_size(0),
_completed_task_count(0),
_not_full(_push_lock),
_not_empty(_pop_lock)
{

}

template<typename Task>
inline 
ThreadPoolExecutor<Task>::~ThreadPoolExecutor()
{
    while(!_to_unregister_workers->empty())
    {
        Worker* w = _to_unregister_workers->front();
        _to_unregister_workers->pop();

        _workers.erase(w);
        delete w;
        w = NULL;
    }

    typename WorkerSet::iterator iter =  _workers.begin();

    for(; iter!=_workers.end(); iter++)
    {
        (*iter)->cancel();
        //delete(*iter);
    }

    _workers.clear();
    
    if(_task_queue)
    {
         delete   _task_queue;
         _task_queue = NULL;
    }

    if(_to_unregister_workers)
    {
         delete   _to_unregister_workers;
         _to_unregister_workers = NULL;
    }    
}

template<typename Task>
inline 
int ThreadPoolExecutor<Task>::init()
{
    if((_core_pool_size <= 0) ||(_maximum_pool_size <= 0) || (_maximum_pool_size < _core_pool_size))
    {
        return -1;
    }

    _task_queue = new TaskQueue(_core_pool_size * TASK_QUEUE_FACTOR);
    _to_unregister_workers = new UnregisterQueue(_maximum_pool_size);

    return 0;
}

template<typename Task>
inline 
int ThreadPoolExecutor<Task>::execute(Task& task)
{
    //process unregistered workers
    while(!_to_unregister_workers->empty())
    {
        Worker* w = _to_unregister_workers->front();
        _to_unregister_workers->pop();

        _workers.erase(w);
        delete w;
        w = NULL;
        --_pool_size;
    }

    if ((_pool_size < _core_pool_size) && 
         add_if_under_maximum_pool_size(task))
    {
        return 0;    
    }

    if (_task_queue->push(task) == 0)
    {
        if((_task_queue->size() == 1) &&_pop_lock.try_acquire(0))
        {
            _not_empty.signal();
            _pop_lock.release();
        }
        
        return 0;
    }

    if( add_if_under_maximum_pool_size(task) )
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

template<typename Task>
inline 
int ThreadPoolExecutor<Task>::get_task(Task& task)
{
    for(;;)
    {
        if (_pool_size <= _core_pool_size)   // untimed wait if core
        {
            _pop_lock.acquire();
            while(_task_queue->empty())
            {
                _not_empty.wait(100);
            }
            
            task = _task_queue->front();
            _task_queue->pop();
            _pop_lock.release();

            return 0;
            
        }
    }
/*
        long timeout = _keep_alive_time;
        if (timeout <= 0) // die immediately for 0 timeout
        {
            return -1;
        }

        
        bool signaled;

        _pop_lock.acquire();
        while(_task_queue->empty())
        {
            signaled = _not_empty.wait(timeout);
        }

        if(signaled)
        {
            task = _task_queue->front();
            _task_queue->pop();

            _pop_lock.release();
            return 0;
        }
        _pop_lock.release();     

        
        if (_pool_size > _core_pool_size) // timed out
        {
            return -1;
        }
        
        // else, after timeout, pool shrank so shouldn't die, so retry
    }
    */
}




/**
 * Create and start a new thread running firstTask as its first
 * task, only if fewer than corePoolSize threads are running.
 * @param firstTask the task the new thread should run first (or
 * null if none)
 * @return true if successful.
 */
template<typename Task>
inline 
bool ThreadPoolExecutor<Task>::add_if_under_maximum_pool_size(Task& first_task) 
{
    Thread* t = NULL;

    if (_pool_size < _maximum_pool_size)
    {
            t = add_worker(first_task);
    }

    if (t == NULL)
    {
        return false;
    }
    
    int nt = ++_pool_size;
    
    if (nt > _largest_pool_size)
    {
        _largest_pool_size = nt;
    }
    
    t->start();
    
    return true;

}    

#endif
