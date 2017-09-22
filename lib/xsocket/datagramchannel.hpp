#ifndef _BASE_NET_DATAGRAMCHANNEL_H_
#define _BASE_NET_DATAGRAMCHANNEL_H_

#include "socketaddress.hpp"
#include "selectablechannel.hpp"
#include "socketchannel.hpp"
#include "bytebuffer.hpp"
#include "bytechannel.hpp"

class DatagramChannel : public SelectableChannel, public ByteChannel
{
private:
    int fd;

    // State, increases monotonically

    static  int ST_UNINITIALIZED;
    static  int ST_UNCONNECTED;
    static  int ST_CONNECTED;
    static  int ST_KILLED;

    int state;

    // Binding
    SocketAddress* _local_address;
    SocketAddress* _remote_address;

    // Options, created on demand
    //SocketOpts.IP.TCP options;


public:
    // Constructor for normal connecting sockets
    //
    DatagramChannel(SelectorProvider* sp);

    // Constructor for sockets obtained from server sockets
    //
    DatagramChannel(SelectorProvider* sp, int fd, InetSocketAddress& remote);

    /**
     * Opens a socket channel.
     *
     * <p> The new channel is created by invoking the {@link
     * java.nio.channels.spi.SelectorProvider#openSocketChannel
     * openSocketChannel} method of the system-wide default {@link
     * java.nio.channels.spi.SelectorProvider} object.  </p>
     *
     * @return  A new socket channel
     *
     * @throws  IOException
     *  		If an I/O error occurs
     */
    static DatagramChannel* open();
    /**
     * Opens a socket channel and connects it to a remote address.
     *
     * <p> This convenience method works as if by invoking the {@link #open()}
     * method, invoking the {@link #connect(SocketAddress) connect} method upon
     * the resulting socket channel, passing it <tt>remote</tt>, and then
     * returning that channel.  </p>
     *
     * @param  remote
     *  	   The remote address to which the new channel is to be connected
     *
     * @throws  AsynchronousCloseException
     *  		If another thread closes this channel
     *  		while the connect operation is in progress
     *
     * @throws  ClosedByInterruptException
     *  		If another thread interrupts the current thread
     *  		while the connect operation is in progress, thereby
     *  		closing the channel and setting the current thread's
     *  		interrupt status
     *
     * @throws  UnresolvedAddressException
     *  		If the given remote address is not fully resolved
     *
     * @throws  UnsupportedAddressTypeException
     *  		If the type of the given remote address is not supported
     *
     * @throws  SecurityException
     *  		If a security manager has been installed
     *  		and it does not permit access to the given remote endpoint
     *
     * @throws  IOException
     *  		If some other I/O error occurs
     */
    static DatagramChannel* open(SocketAddress& remote);

    /**
     * Returns an operation set identifying this channel's supported
     * operations.
     *
     * <p> Socket channels support connecting, reading, and writing, so this
     * method returns <tt>(</tt>{@link SelectionKey#OP_CONNECT}
     * <tt>|</tt>&nbsp;{@link SelectionKey#OP_READ} <tt>|</tt>&nbsp;{@link
     * SelectionKey#OP_WRITE}<tt>)</tt>.  </p>
     *
     * @return  The valid-operation set
     */
    int valid_ops();
    bool ensureOpen();

    int send(ByteBuffer& src, SocketAddress& target);
    int receive(ByteBuffer& dst, SocketAddress&);
    /**
     * @throws  NotYetConnectedException
     *  		If this channel is not yet connected
     */
    int read(ByteBuffer& dst);

    /**
     * @throws  NotYetConnectedException
     *  		If this channel is not yet connected
     */
    //long read(ByteBuffer[] dsts, int offset, int length);

    /**
     * @throws  NotYetConnectedException
     *  		If this channel is not yet connected
     */
    //long read(ByteBuffer[] dsts);

    /**
     * @throws  NotYetConnectedException
     *  		If this channel is not yet connected
     */
    int write(ByteBuffer& src);

    /**
     * @throws  NotYetConnectedException
     *  		If this channel is not yet connected
     */
    // long write(ByteBuffer[] srcs, int offset, int length);

    /**
     * @throws  NotYetConnectedException
     *  		If this channel is not yet connected
     */
    //long write(ByteBuffer[] srcs);

    //SocketOpts options();

    bool is_bound() ;
    SocketAddress* local_address();

    SocketAddress* remote_address();
    int bind(SocketAddress& local) ;

    bool is_connected();
    void ensure_open_and_unconnected();
    bool connect(SocketAddress& sa);
    DatagramChannel& disconnect();


    // AbstractInterruptibleChannel synchronizes invocations of this method
    // using AbstractInterruptibleChannel.closeLock, and also ensures that this
    // method is only ever invoked once.  Before we get to this method, is_open
    // (which is volatile) will have been set to false.
    //
    void impl_close_selectablechannel();
    void kill();

    /**
     * Translates native poll revent ops into a ready operation ops
     */
    bool translate_ready_ops(int ops, int initialOps, SelectionKey& sk) ;
    bool translate_and_update_ready_ops(int ops, SelectionKey& sk);
    bool translate_and_set_ready_ops(int ops, SelectionKey& sk);
    bool translate_and_set_interest_ops(int ops, SelectionKey& sk);

    int get_fd();

private:
    //long read0(ByteBuffer[] bufs);
    //long write0(ByteBuffer[] bufs) ;
    void impl_configure_blocking(bool block);
};

#endif

