#ifndef __MONITOR_H__
#define __MONITOR_H__

#include "status.hpp"
#include "fast_lock.hpp"



/**
 * @class Monitor
 * @author Eric Crahen <http://www.code-foo.com>
 * @date <2003-07-18T08:16:09-0400>
 * @version 2.2.8
 */
class Monitor
            : public Status,
            private NonCopyable
{
private:

    //! Serialize access to external objects
    FastLock _lock;

    //! Condition variable used to block a thread.
    pthread_cond_t _waitCond;

    //! Serialize access to the internal state of the monitor
    pthread_mutex_t _waitLock;

    //! Owning thread
    pthread_t _owner;

    //! Waiting flag, to avoid uneccessary signals
    volatile bool _waiting;

public:

    typedef Status::STATE STATE;

    //! Create a new monitor.
    Monitor();

    //! Destroy the monitor.
    ~Monitor();

    //! Acquire the lock for this monitor.
    inline void acquire()
    {
        _lock.acquire();
    }

    //! Acquire the lock for this monitor.
    inline bool try_acquire()
    {
        return _lock.try_acquire();
    }

    //! Release the lock for this monitor
    inline void release()
    {
        _lock.release();
    }

    /**
     * Wait for a state change and atomically unlock the external lock.
     * Blocks for an indefinent amount of time.
     *
     * @return INTERRUPTED if the wait was ended by a interrupt()
     *         or SIGNALED if the wait was ended by a notify()
     *
     * @post the external lock is always acquired before this function returns
     */
    inline STATE wait()
    {
        return wait(0);
    }

    /**
     * Wait for a state change and atomically unlock the external lock.
     * May blocks for an indefinent amount of time.
     *
     * @param timeout - maximum time to block (milliseconds) or 0 to
     * block indefinently
     *
     * @return INTERRUPTED if the wait was ended by a interrupt()
     *         or TIMEDOUT if the maximum wait time expired.
     *         or SIGNALED if the wait was ended by a notify()
     *
     * @post the external lock is always acquired before this function returns
     */
    STATE wait(unsigned long timeout);

    /**
     * Interrupt this monitor. If there is a thread blocked on this monitor object
     * it will be signaled and released. If there is no waiter, a flag is set and
     * the next attempt to wait() will return INTERRUPTED w/o blocking.
     *
     * @return false if the thread was previously INTERRUPTED.
     */
    bool interrupt();

    /**
     * Notify this monitor. If there is a thread blocked on this monitor object
     * it will be signaled and released. If there is no waiter, a flag is set and
     * the next attempt to wait() will return SIGNALED w/o blocking, if no other
     * flag is set.
     *
     * @return false if the thread was previously INTERRUPTED.
     */
    bool notify();

    /**
     * Check the state of this monitor, clearing the INTERRUPTED status if set.
     *
     * @return bool true if the monitor was INTERRUPTED.
     * @post INTERRUPTED flag cleared if the calling thread owns the monitor.
     */
    bool is_interrupted();

    /**
     * Mark the Status CANCELED, and INTERRUPT the montor.
     *
     * @see interrupt()
     */
    bool cancel();

    /**
     * Test the CANCELED Status, clearing the INTERRUPTED status if set.
     *
     * @return bool
     */
    bool is_canceled();

};


#endif
