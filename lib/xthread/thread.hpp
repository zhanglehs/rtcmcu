#ifndef __THREADIMPL_H__
#define __THREADIMPL_H__
#include <pthread.h>
#include <map>
#include <deque>

#include "cancelable.hpp"
#include "priority.hpp"
#include "noncopyable.hpp"
#include "runnable.hpp"
#include "waitable.hpp"

#include "monitor.hpp"
#include "state.hpp"
#include "tss.hpp"




class  Thread
            : public Cancelable,
            public Waitable,
            public NonCopyable,
            public  Runnable
{
    pthread_t  _tid;

    typedef std::deque<Thread*> List;

    //! TSS to store implementation to current thread mapping.
    static TSS<Thread*> _thread_map;
    //! The Monitor for controlling this thread
    Monitor _monitor;

    //! Current state for the thread
    State _state;

    //! Joining threads
    List _joiners;

    Runnable* _target;
	
private:

    //! Cached thread priority
    Priority _priority;

    //! Request cancel() when main() goes out of scope
    bool _auto_cancel;

	pthread_cond_t _qready;
	pthread_mutex_t _qlock;

public:

    Thread();
	
    Thread(Runnable&, bool);
	int do_construct()
	{
		int r1=pthread_cond_init(&_qready,NULL);
		int r2=pthread_mutex_init(&_qlock,NULL);
		if(r1<0 || r2<0)
			return -1;
		return 0;
	}
	
    virtual ~Thread();
	void block()
	{
		pthread_mutex_lock(&_qlock);
		pthread_cond_wait(&_qready,&_qlock);
		pthread_mutex_unlock(&_qlock);
	}
	void timed_block(unsigned sec)
	{
		timespec   mytime;
		mytime.tv_sec=time(NULL)+sec;
		mytime.tv_nsec=0;
		pthread_mutex_lock(&_qlock);
		pthread_cond_timedwait(&_qready,&_qlock,&mytime);
		pthread_mutex_unlock(&_qlock);
	}
	void unblock()
	{
		pthread_cond_signal(&_qready);
	}
    void start();
	pthread_t get_tid()
	{
		return _tid;
	}
    virtual void run();
    /**
     * Wait for the thread represented by this object to complete its task.
     * The calling thread is blocked until the thread represented by this
     * object exits.
     *
     * @exception Deadlock_Exception thrown if thread attempts to join itself
     * @exception InvalidOp_Exception thrown if the thread cannot be joined
     * @exception Interrupted_Exception thrown if the joining thread has been interrupt()ed
     */
    void wait();

    /**
     * Wait for the thread represented by this object to complete its task.
     * The calling thread is blocked until the thread represented by this
     * object exits, or until the timeout expires.
     *
     * @param timeout maximum amount of time (milliseconds) this method
     *        could block the calling thread.
     *
     * @return
     *   - <em>true</em> if the thread task complete before <i>timeout</i>
     *     milliseconds elapse.
     *   - <em>false</em> othewise.
     *
     * @exception Deadlock_Exception thrown if thread attempts to join itself
     * @exception InvalidOp_Exception thrown if the thread cannot be joined
     * @exception Interrupted_Exception thrown if the joining thread has been interrupt()ed
     */
    bool wait(unsigned long timeout);
    /**
     * Interrupt and cancel this thread in a single operation. The thread will
     * return <em>true</em> whenever its cancelation status is tested in the future.
     *
     * @exception InvalidOp_Exception thrown if a thread attempts to cancel itself
     *
     * @see Thread::interrupt()
     * @see Cancelable::cancel()
     */
    void cancel();
    
    bool is_daemon();
    void  set_daemon();

    Monitor& get_monitor();

    bool interrupt();

    bool is_interrupted();

    bool is_canceled();

    Priority get_priority() const;
    void set_priority(Priority);


    bool join(unsigned long);

    bool is_active();

    static void sleep(unsigned long);

    static void yield();

    static Thread* current();
    static bool is_current(Thread* t)
    {
        return pthread_equal(pthread_self(), t->_tid);
    }
    static void activate(Thread* ops) ;
    void   dispatch();
private:
    bool spawn();

};



#endif
