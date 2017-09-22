#ifndef __DEFERREDINTERRUPTIONSCOPE_H__
#define __DEFERREDINTERRUPTIONSCOPE_H__

#include <cassert>
#include "guard.hpp"
#include "thread.hpp"


/**
 * @class DeferredInterruptionScope
 * @author Eric Crahen <http://www.code-foo.com>
 * @date <2003-07-16T19:45:18-0400>
 * @version 2.3.0
 *
 * Locking policy for a Guard that will defer any state reported
 * for the reported Status of a thread except SIGNALED until the
 * scope has ended. This allows a Guard to be used to create an
 * uninterruptible region in code.
 */
class DeferredInterruptionScope
{
public:

    template <class LockType>
    static void createScope(LockHolder<LockType>& l) {

        l.getLock().interest(Monitor::SIGNALED);

    }

    template <class LockType>
    static void destroyScope(LockHolder<LockType>& l) {

        l.getLock().interest(Monitor::ANYTHING);

    }

};


#endif
