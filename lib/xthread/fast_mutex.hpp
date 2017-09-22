#ifndef _FAST_MUTEX_HPP_
#define _FAST_MUTEX_HPP_

#include "lockable.hpp"
#include "noncopyable.hpp"



  class FastLock;

  /**
   * @class FastMutex
   * @author Eric Crahen <http://www.code-foo.com>
   * @date <2003-07-19T18:45:39-0400>
   * @version 2.2.0
   *
   * A FastMutex is a small fast implementation of a non-recursive, mutually exclusive
   * Lockable object. This implementation is a bit faster than the other Mutex classes
   * as it involved the least overhead. However, this slight increase in speed is 
   * gained by sacrificing the robustness provided by the other classes. 
   *
   * A FastMutex has the useful property of not being interruptable; that is to say  
   * that acquire() and tryAcquire() will not throw Interrupted_Exceptions.
   *
   * @see Mutex
   *
   * <b>Scheduling</b>
   *
   * Scheduling is left to the operating systems and may vary.
   *
   * <b>Error Checking</b>
   *
   * No error checking is performed, this means there is the potential for deadlock.
   */ 
  class  FastMutex : public Lockable, private NonCopyable 
  {
    
    FastLock* _lock;

  public:
  
    //! Create a FastMutex
    FastMutex();
  
    //! Destroy a FastMutex
    virtual ~FastMutex();
  
    /**
     * Acquire exclusive access to the mutex. The calling thread will block until the 
     * lock can be acquired. No safety or state checks are performed.
     * 
     * @pre The calling thread should <i>not</i> have previously acquired this lock.
     *      Deadlock will result if the same thread attempts to acquire the mutex more 
     *      than once. 
     *
     * @post The calling thread obtains the lock successfully if no exception is thrown.
     * @exception Interrupted_Exception never thrown
     */
    virtual void acquire();
  
    /**
     * Release exclusive access. No safety or state checks are performed.
     * 
     * @pre the caller should have previously acquired this lock
     */
    virtual void release();
  
    /**
     * Try to acquire exclusive access to the mutex. The calling thread will block until the 
     * lock can be acquired. No safety or state checks are performed.
     * 
     * @pre The calling thread should <i>not</i> have previously acquired this lock.
     *      Deadlock will result if the same thread attempts to acquire the mutex more 
     *      than once. 
     *
     * @param timeout unused
     * @return 
     * - <em>true</em> if the lock was acquired
     * - <em>false</em> if the lock was acquired
     *
     * @post The calling thread obtains the lock successfully if no exception is thrown.
     * @exception Interrupted_Exception never thrown
     */
    virtual bool try_acquire(unsigned long timeout);
  
  }; /* FastMutex */



#endif 
