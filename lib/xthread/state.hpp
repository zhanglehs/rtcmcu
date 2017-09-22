#ifndef __STATE_H__
#define __STATE_H__



/**
 * @class State
 * @author Eric Crahen <http://www.code-foo.com>
 * @date <2003-07-16T20:04:01-0400>
 * @version 2.2.1
 *
 * Class to encapsulate the current state of the threads life-cycle.
 */
class State {
public:

    //! Various states
    typedef enum { REFERENCE, IDLE, RUNNING, JOINED } STATE;

    /**
     * Create State with the given flag set.
     */
    State(STATE initialState) : _state(initialState) {}

    /**
     * Test for the IDLE state. No task has yet run.
     */
    bool isIdle() const {
        return _state == IDLE;
    }

    /**
     * Test for the JOINED state. A task has completed and
     * the thread is join()ed.
     *
     * @return bool
     */
    bool isJoined() const {
        return _state == JOINED;
    }

    /**
     * Test for the RUNNING state. A task is in progress.
     *
     * @return bool
     */
    bool isRunning() const {
        return _state == RUNNING;
    }

    /**
     * Test for the REFERENCE state. A task is in progress but not
     * under control of this library.
     *
     * @return bool
     */
    bool isReference() const {
        return _state == REFERENCE;
    }

    /**
     * Transition to the IDLE state.
     *
     * @return bool true if successful
     */
    bool setIdle() {

        if(_state != RUNNING)
            return false;

        _state = IDLE;
        return true;

    }

    /**
     * Transition to the RUNNING state.
     *
     * @return bool true if successful
     */
    bool setRunning() {

        if(_state != IDLE)
            return false;

        _state = RUNNING;
        return true;

    }

    /**
     * Transition to the REFERENCE state.
     *
     * @return bool true if successful
     */
    bool setReference() {

        if(_state != IDLE)
            return false;

        _state = REFERENCE;
        return true;

    }


    /**
     * Transition to the JOINED state.
     *
     * @return bool true if successful
     */
    bool setJoined() {

        _state = JOINED;
        return true;

    }

private:

    //! Current state
    STATE _state;

};


#endif
