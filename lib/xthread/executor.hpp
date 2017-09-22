#ifndef _EXECUTOR_H_
#define _EXECUTOR_H_


template<typename Task>
class Executor
{
public:

/**
 * If supported by the Executor, interrupt all tasks submitted prior to 
 * the invocation of this function.
 */    
//virtual void interrupt() = 0;

/**
 * Submit a task to this Executor. 
 *
 * @param task Task to be run by a thread managed by this executor 
 *
 * @pre  The Executor should have been canceled prior to this invocation.
 * @post The submitted task will be run at some point in the future by this Executor.
 *
 * @exception Cancellation_Exception thrown if the Executor was canceled prior to
 *            the invocation of this function.
 */
virtual int execute(const Task& task) = 0;

};



#endif 
