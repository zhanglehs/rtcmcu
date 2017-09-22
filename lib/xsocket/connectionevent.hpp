#ifndef _CONNECTION_EVENT_HPP_
#define  _CONNECTION_EVENT_HPP_
#include <string>

using namespace std;

class ConnectionInfo;
/**
 * An event occurring on a connection that affects its
 * state and may affect message processing.
 */
class ConnectionEvent
{
    /**
     * The connection has initially connected.
     */
public:
    enum
    {
        CONNECTION_CONNECTED		= 1,
        /**
        			* The connection has been closed. This event may
        			* be preceded by an error condition event, if the
        			* closure was abnormal.
        			*/
        CONNECTION_CLOSED			= 2,
        /**
        			* The I/O processor has detected an IOException while
        			* interacting with the connection.
        			*/
        CONNECTION_IO_EXCEPTION		= 3,
        /**
        			* A reconnect handler has successfully reconnected the connection
        			* following an abnormal shutdown.
        			*/
        CONNECTION_RECONNECTED		= 4,
        /**
        			* The datagram processor cannot interpret the byte stream
        			* because the protocol is invalid.
        			*/
        CONNECTION_INVALID_PROTOCOL	= 5,
    };

    /// PRIVATE MEMBERS

private:
    ConnectionInfo* _connection;
    int _event;

    /**
     * Creates a connection event related to the given
     * connection.
     *
     * @param conn the connection
     * @param event the event code, one of the CONNECTION_xxx constants.
     */
public:
    ConnectionEvent(ConnectionInfo& conn, int event)
    {
        _connection = &conn;
        _event = event;
    }

    /**
     * Returns the event code.
     * @return an event code, one of CONNECTION_xxx.
     */
    int getEventCode()
    {
        return _event;
    }

    /**
     * Returns true if the connection event represents a
     * failure mode.
     */
    bool isFailure()
    {
        return (_event == CONNECTION_IO_EXCEPTION || _event == CONNECTION_INVALID_PROTOCOL);
    }

    /**
     * Obtains the connection reference.
     * @return a JMS connection object. Generally, this can
     * be cast into an IConnectionInfo, if necessary.
     * @see com.ubermq.kernel.IConnectionInfo
     */
    ConnectionInfo& getConnection()
    {
        return *_connection;
    }

    string toString()
    {
        switch (getEventCode())
        {
        case ConnectionEvent::CONNECTION_CLOSED:
            return "CONNECTION_CLOSED";
        case ConnectionEvent::CONNECTION_CONNECTED:
            return "CONNECTION_CONNECTED";
        case ConnectionEvent::CONNECTION_INVALID_PROTOCOL:
            return "CONNECTION_INVALID_PROTOCOL";
        case ConnectionEvent::CONNECTION_IO_EXCEPTION:
            return "CONNECTION_IO_EXCEPTION";
        case ConnectionEvent::CONNECTION_RECONNECTED:
            return "CONNECTION_RECONNECTED";
        default:
            return "UNKNOWN EVENT";
        }
    }
};

#endif

