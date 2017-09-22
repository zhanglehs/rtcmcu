#ifndef _DATAGRAM_PROC_
#define _DATAGRAM_PROC_

#include <stdint.h>
#include <map>
#include <hash_map>
#include <set>
#include "imessageprocessor.hpp"
#include "connectioninfo.hpp"
#include "shared_memory_transport.hpp"
#include "datagramfactory.hpp"

using namespace std;
using namespace commoncpp;


typedef map<uint32_t, ConnectionInfo* > ConnectionMap;
typedef set<ConnectionInfo* > ConnectionSet;
typedef hash_map<uint32_t, shared_memory_transport*> TopicidChannelMap;
typedef set<shared_memory_transport* > IOChannelSet;
class DatagramProc : public IMessageProcessor
{
    // the standard overflow handler for outgoing messages
private:
    enum
    {
        DEFAULT_MAX_CONNECTIONS = 1024,
        DEFAULT_MAX_IN_DATAGRAMS = 1024,
    };
    // static  long INITIAL_TIMEOUT = Long.valueOf(Configurator.getProperty(ServerConfig.DGP_INITIAL_TIMEOUT, "50")).longValue();
    //  static  long MAXIMUM_TIMEOUT = Long.valueOf(Configurator.getProperty(ServerConfig.DGP_MAXIMUM_TIMEOUT, "5000")).longValue();
    //  static  int TIMEOUT_FACTOR = Integer.valueOf(Configurator.getProperty(ServerConfig.DGP_BACKOFF_MULTIPLIER, "2")).intValue();

    // or, a user-defined overflow handler available on classpath.
    //   static  String OVERFLOW_HANDLER_CLASS = Configurator.getProperty(ServerConfig.DGP_OVERFLOW_HANDLER, "");
    //   static  String OVERFLOW_HANDLER_INIT = Configurator.getProperty(ServerConfig.DGP_OVERFLOW_HANDLER_INIT, "");

    // routers. the normal router contains DatagramSink objects.
    // the ack router only routes locally and requires IDatagramEndpoint objects.
    string  _root_path;
    ConnectionSet _connections;
    ConnectionMap _idConnections;
    uint32_t  _maxConnetions;


    // statistical variables
    //int nMessagesIn, nMessagesOut, nMessagesDropped;
    //int accumLatency;
    //long startTime;

    // my overflow handler
    IOverflowHandler overflow;

    TopicidChannelMap                      _topicidChannels;
    IOChannelSet                              _ioChannels;
    /**
     * Creates a datagram processor with a journal to restore state, and
     * a datagram factory used to interpret the incoming message stream
     * and produce well-formed output.
     *
     * @param journal the journal used to store state across invocations of the
     * processor.
     * @param factories the datagram factory holder for creating datagrams
     * @param handler the overflow handler to use when overflow conditions
     * occur. If this is null, the configurator is consulted. If the configurator
     * does not specify a handler, the default is used.
     */
public:
    DatagramProc(string root_path);
    DatagramProc(IOverflowHandler& handler, uint32_t maxConnetions = DEFAULT_MAX_CONNECTIONS);
    /*
    {

    				// set up an overflow handler
    				try
    				{
    					if (OVERFLOW_HANDLER_CLASS.length() > 0)
    					{
    						Class overflowClass = Class.forName(OVERFLOW_HANDLER_CLASS);
    						Constructor c = overflowClass.getConstructor(new Class[] {String.class});
    						overflow = (IOverflowHandler)c.newInstance(new Object[] {OVERFLOW_HANDLER_INIT});
    					}
    					else
    					{
    						throw new InstantiationException();
    					}
    				}
    				catch (Exception e)
    				{
    					// use mine, a special TTL aware overflow handler.
    					// In UMQ 2.3, we now favor receivers in most cases and
    					// we kill rogue senders that may be over-publishing.
    					// it falls on the sender to regulate itself.
    					//
    					// The Old behavior can be resurrected by specifying the KillReceiver
    					// overflow handler in the OVERFLOW_HANDLER_CLASS runtime parameter.
    					overflow = new TTLOverflowHandler(INITIAL_TIMEOUT,
    													  TIMEOUT_FACTOR,
    													  MAXIMUM_TIMEOUT,
    													  true);
    				}

    }
     	*/
    virtual ~DatagramProc(){}
    int init();
    int frame(ByteBuffer& bb);
    void accept(ConnectionInfo& ci);
    void remove(ConnectionInfo& ci);
    void process(ConnectionInfo& conn, ByteBuffer& packet);
    int  poll(uint32_t timeout = 0);
private:
    int poll_channel(shared_memory_transport& channel);
private:
    //statistics
    uint32_t m_recv_num;
    uint32_t m_recv_fwd_num;
    uint32_t m_send_num;
    uint32_t m_send_fwd_num;
};


#endif


