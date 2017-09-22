#ifndef __CANCELABLE_H__
#define __CANCELABLE_H__

#include "exceptions.hpp"


/**
 * @class Cancelable
 *
 * @author Eric Crahen <http://www.code-foo.com>
 * @date <2003-07-16T09:28:46-0400>
 * @version 2.3.0
 *
 * The Cancelable interface defines a common method of adding general <i>disable-and-exit</i>
 * semantics to some object. By cancel()ing a Cancelable object, a request is
 * made to disable that object.
 *
 * <b>Disabling</b>
 *
 * A cancel()ed object may not necessarily abort it work immediately. Often, it much more
 * elegant for a cancel()ed object to complete handling whatever responsibilities have
 * been assigned to it, but it will <i>not</i> take on any new responsibility.
 *
 * <b>Exiting</b>
 *
 * A cancel()ed should complete its responsibilities as soon as possible.
 * Canceling is not only a request to stop taking on new responsibility, and to
 * complete its current responsibility. Its also a request to complete dealing with its
 * current responsibilities, quickly when possible.
 */
class Cancelable
{
public:

    //! Destroy a Cancelable object.
    virtual ~Cancelable() {}

    /**
     * Canceling a Cancelable object makes a request to disable that object.
     * This entails refusing to take on any new responsibility, and completing
     * its current responsibilities quickly.
     *
     * Canceling an object more than once has no effect.
     *
     * @post The Cancelable object will have permanently transitioned to a
     *       disabled state; it will now refuse to accept new responsibility.
     */
    virtual void cancel() = 0;

    /**
     * Determine if a Cancelable object has been canceled.
     *
     * @return
     *   - <em>true</em> if cancel() was called prior to this function.
     *   - <em>false</em> otherwise.
     */
    virtual bool is_canceled() = 0;

}; /* Cancelable */




#endif
