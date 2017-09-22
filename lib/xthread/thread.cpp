#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <bits/stl_algo.h>
#include "runnable.hpp"
#include "thread.hpp"
#include "deferred_interruption_scope.hpp"

using namespace std;

extern "C" void *_dispatch(void *arg);

TSS<Thread*> Thread::_thread_map;
  
Thread::Thread()
        :_tid(0), 
        _state(State::IDLE),
        _target(NULL),
        _priority(Medium),        
        _auto_cancel(false)        
{
	if(do_construct()<0)
		throw "zthread error";

}

Thread::Thread(Runnable& target, bool auto_cancel)
        :_tid(0),
        _state(State::IDLE),
        _target(&target),
        _priority(Medium),        
        _auto_cancel(auto_cancel)        
{
	if(do_construct()<0)
		throw "zthread error";
}


Thread::~Thread()
{
    if(is_active())
    {
        abort();
    }
}

Monitor& Thread::get_monitor()
{
    return _monitor;
}

void Thread::cancel()
{
    if(Thread::current() == this)
    {
        throw InvalidOp_Exception();
    }
    _monitor.cancel();
    pthread_cancel(_tid);
    _state.setIdle();
    printf("cancel tid %d\n", (int)_tid);
}

bool Thread::interrupt()
{
    return _monitor.interrupt();
}

bool Thread::is_interrupted()
{
    return _monitor.is_interrupted();
}

bool Thread::is_canceled()
{
    return _monitor.is_canceled();
}

Priority Thread::get_priority() const
{
    return _priority;
}

/**
 * Join the thread, blocking the caller until it is interrupted or until
 * the thread represented by this object exits.
 *
 * Reference threads are not under the control of ZThreads and cannot be
 * joined.
 */
bool Thread::join(unsigned long timeout)
{

    // Serial access to this Thread's state
    Guard<Monitor> g1(_monitor);

    // Make sure a thread is not trying to join() itself.
    if(is_current(this))
        throw Deadlock_Exception("Cannot join self.");

    // Reference threads can't be joined.
    if(_state.isReference())
        throw InvalidOp_Exception("Can not join this thread.");

    /*

    TODO: Insert cyclic join check.

    */

    // If the task has not completed yet, wait for completion
    if(!_state.isJoined()) {

        // Add the current thread to the joiner list
        Thread* impl = current();
        _joiners.push_back(impl);

        Monitor::STATE result;

        { // Release this Thread's lock while the joiner sleeps

            _monitor.release();
            Guard<Monitor> g3(impl->get_monitor());

            result = impl->_monitor.wait(timeout);

            _monitor.acquire();

        }

        // Update the joiner list
        List::iterator i = std::find(_joiners.begin(), _joiners.end(), impl);
        if(i != _joiners.end())
            _joiners.erase(i);


        switch(result) {

        case Monitor::TIMEDOUT:
            return false;

        case Monitor::INTERRUPTED:
            throw Interrupted_Exception();

        default:
            break;

        }

    }

    return true;

}

void Thread::wait()
{
    join(0);
}

bool Thread::wait(unsigned long timeout)
{
    return join(timeout == 0 ? 1 : timeout);
}

/**
 * Translate the priority into a pthread value, and update the thread priority.
 *
 * This is not available on all platforms, and probably works differently
 * the platforms that do support it. Pthreads does not have very portable
 * priority support as far I am aware.
 *
 * If SCHED_OTHER is not supported priority values are still set but
 * dont not actually in affect anything.
 *
 * @param prio PRIORITY value
 *
 * @exception Killed_Exception thrown by KILLED threads.
 * @exception InvalidOp_Exception thrown by IDLE, JOINING or JOINED threads.
 */
void Thread::set_priority(Priority p)
{
    Guard<Monitor> g(_monitor);

    // Only set the native priority when the thread is running
    if(_state.isRunning())
    {
        struct sched_param param;

        switch(p) {
        case Low:
            param.sched_priority = 0;
            break;
        case High:
            param.sched_priority = 10;
            break;
        case Medium:
        default:
            param.sched_priority = 5;
        }

        pthread_setschedparam(_tid, SCHED_OTHER, &param);
    }

    _priority = p;
}


/**
 * Test the state Monitor of this thread to determine if the thread
 * is an active thread created by zthreads.
 *
 * @return bool indicating the activity of the thread.
 */
bool Thread::is_active()
{
    Guard<Monitor> g(_monitor);
    return _state.isRunning();
}


/**
 * Get a reference to an implmenetation that maps to the current thread.
 * Accomplished by checking the TLS map. This will always return a valid
 * Thread instance.
 *
 * @return Thread* current implementation that maps to the
 * executing thread.
 */
Thread* Thread::current()
{
      // Get the Thread previously mapped onto the executing thread.
      Thread* impl = _thread_map.get();

      // Create a reference thread for any threads that have been 'discovered'
      // because they are not created by ZThreads.
      if(impl == 0) {

          // Create a Thread to represent this thread.
          impl = new Thread();
          impl->_state.setReference();

          activate(impl);

          // Map a reference thread and insert it into the queue
          _thread_map.set(impl);

      }

      assert(impl != 0);
      return impl;
    
}


void Thread::activate(Thread* ops) 
{

    assert(ops);
    assert(ops->_tid == 0);

    ops->_tid = pthread_self();
    
}
/**
 * Make current thread sleep for the given number of milliseconds.
 * This sleep can be interrupt()ed.
 *
 * @param ms timeout for the sleep.
 *
 * @post the calling thread is blocked by waiting on the internal condition
 * variable. This can be signaled in the monitor of an interrupt
 */
void Thread::sleep(unsigned long ms)
{

    // Make sleep()ing for 0 milliseconds equivalent to a yield.
    if(ms == 0)
    {
        yield();
        return;
    }

    // Get the monitor for the current thread
    Monitor& monitor = current()->get_monitor();

    // Acquire that threads Monitor with a Guard
    Guard<Monitor> g(monitor);

    for(;;)
    {

        switch(monitor.wait(ms))
        {
        case Monitor::INTERRUPTED:

        default:
            return;

        }
    }
}


/**
 * Yield the current timeslice to another thread.
 * If sched_yield() is available it is used.
 * Otherwise, the state Monitor for this thread is used to simiulate a
 * yield by blocking for 1  millisecond, which should give the
 * scheduler a chance to schedule another thread.
 */
void Thread::yield()
{
    // Try to yield with the native operation. If it fails, then
    // simulate with a short wait() on the monitor.
    bool result = false;

#if defined(HAVE_SCHED_YIELD)
    result = sched_yield() == 0;
#endif

    if(!result)
    {

        // Get the monitor for the current thread
        Monitor& monitor = current()->get_monitor();

        // Attempt a wait().
        Guard<Monitor> g(monitor);
        monitor.wait(1);

    }

}

void Thread::start()
{

    Guard<Monitor> g1(_monitor);

    // A Thread must be idle in order to be eligable to run a task.
    if(!_state.isIdle())
        throw InvalidOp_Exception("Thread is not idle.");

    _state.setRunning();

    // Spawn a new thread, blocking the parent (current) thread until
    // the child starts.
    // Attempt to start the child thread
    // Guard<Monitor> g2(parent->_monitor);

    if(!spawn())
    {

        // Return to the idle state & report the error if it doesn't work out.
        _state.setIdle();
        throw Synchronization_Exception();


    }

    // Wait, uninterruptably, for the child's signal. The parent thread
    // still can be interrupted and killed; it just won't take effect
    // until the child has started.

    //Guard<Monitor, DeferredInterruptionScope> g3(parent->_monitor);

    //if(parent->_monitor.wait() != Monitor::SIGNALED) {
    // assert(0);
    //}


}
void Thread::run()
{
    if (_target != NULL)
    {
        _target->run();
    }
}



bool Thread::spawn()
{
    return pthread_create(&_tid, 0, _dispatch, this) == 0;
}


void   Thread::dispatch()
{
    pthread_detach(_tid);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
    _state.setReference();

    // Map a reference thread and insert it into the queue
    _thread_map.set(this);

    run();
    
    // hechao add
    _state.setIdle();

    // Exit the thread
    pthread_exit((void**)0);
}


extern "C" void *_dispatch(void *arg)
{   
    Thread* thread = (Thread*)arg;

    thread->dispatch();

    return (void*)0;
}

