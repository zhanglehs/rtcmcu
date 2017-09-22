#ifndef _CONNECTION_EVENT_LISTENER_HPP_
#define _CONNECTION_EVENT_LISTENER_HPP_

#include <set>

using namespace std;

#include "connectionevent.hpp"
/**
 * A listener for connection events.
 */
class ConnectionEventListener
{
    /**
     * Processes a connection event, encoded in the event object.
     *
     * @param e a connection event
     */
public:
    ConnectionEventListener(){}
    virtual ~ConnectionEventListener(){}
    virtual void connection_event(ConnectionEvent& e) = 0;
};

typedef	set<ConnectionEventListener*> ConnectionEventListernerSet;
#endif
