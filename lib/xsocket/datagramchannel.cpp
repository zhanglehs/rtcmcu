#include <errno.h>
#include <assert.h>
#include <sys/poll.h>
#include "net.hpp"
#include "selector.hpp"
#include "selectorprovider.hpp"
#include "selectionkey.hpp"
#include "datagramchannel.hpp"

int  DatagramChannel::ST_UNINITIALIZED = -1;
int  DatagramChannel::ST_UNCONNECTED = 0;

int  DatagramChannel::ST_CONNECTED = 1;
int  DatagramChannel::ST_KILLED = 2;


DatagramChannel::DatagramChannel(SelectorProvider* sp)
        :SelectableChannel(sp),_local_address(NULL),_remote_address(NULL)
{
    state = ST_UNINITIALIZED;

    //options = NULL;

    this->fd = Net::socket(false);

    this->state = ST_UNCONNECTED;
}

// Constructor for sockets obtained from server sockets
//
DatagramChannel::DatagramChannel(SelectorProvider* sp, int fd , InetSocketAddress& remote)
        :SelectableChannel(sp),_local_address(NULL),_remote_address(NULL)
{
    state = ST_UNINITIALIZED;

    //options = NULL;
    this->state = ST_CONNECTED;
    this->fd = fd;
    InetSocketAddress* isa =  new InetSocketAddress;
    *isa = remote;
    this->_remote_address = isa;

}

/**
 * Opens a socket channel.
 *
 * <p> The new channel is created by invoking the {@link
 * java.nio.channels.spi.SelectorProvider#openDatagramChannel
 * openDatagramChannel} method of the system-wide default {@link
 * java.nio.channels.spi.SelectorProvider} object.  </p>
 *
 * @return  A new socket channel
 *
 * @throws  IOException
 *          If an I/O error occurs
 */
DatagramChannel* DatagramChannel::open()
{
    return SelectorProvider::provider().openDatagramChannel();
}

/**
 * Opens a socket channel and connects it to a remote address.
 *
 * <p> This convenience method works as if by invoking the {@link #open()}
 * method, invoking the {@link #connect(SocketAddress) connect} method upon
 * the resulting socket channel, passing it <tt>remote</tt>, and then
 * returning that channel.  </p>
 *
 * @param  remote
 *         The remote address to which the new channel is to be connected
 *
 * @throws  AsynchronousCloseException
 *          If another thread closes this channel
 *          while the connect operation is in progress
 *
 * @throws  ClosedByInterruptException
 *          If another thread interrupts the current thread
 *          while the connect operation is in progress, thereby
 *          closing the channel and setting the current thread's
 *          interrupt status
 *
 * @throws  UnresolvedAddressException
 *          If the given remote address is not fully resolved
 *
 * @throws  UnsupportedAddressTypeException
 *          If the type of the given remote address is not supported
 *
 * @throws  SecurityException
 *          If a security manager has been installed
 *          and it does not permit access to the given remote endpoint
 *
 * @throws  IOException
 *          If some other I/O error occurs
 */
DatagramChannel* DatagramChannel::open(SocketAddress& remote)
{
    DatagramChannel* sc = open();
    sc->connect(remote);
    return sc;
}

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
int DatagramChannel::valid_ops() {
    return (SelectionKey::OP_READ
            | SelectionKey::OP_WRITE
            | SelectionKey::OP_CONNECT);
}




bool DatagramChannel::ensureOpen()
{
    if (!is_open())
        return false;//throw new ClosedChannelException();
    return true;
}


/**
 * @throws  NotYetConnectedException
 *          If this channel is not yet connected
 */



int DatagramChannel::read(ByteBuffer& bb)
{
    if (!ensureOpen())
    {
        return -1;
    }

    if (!is_open())
    {
        return -1;
    }

    for (;;)
    {
        int pos = bb.position();
        int lim = bb.limit();

        int rem = (pos <= lim ? lim - pos : 0);

        if (rem == 0)
        {
            return -1;
        }

        int n = 0;

        n = ::read(fd, bb.address() + pos, rem);

        if (n > 0)
        {
            bb.position(pos + n);
        }
        return n;
    }

}



/*
    long DatagramChannel::read(ByteBuffer[] dsts) {
	return read(dsts, 0, dsts.length);
    }
	long DatagramChannel::read0(ByteBuffer[] bufs)
	{

        if (bufs == null)
            throw new NullPointerException();
	synchronized (readLock) {
            if (!ensure_read_open())
                return -1;
	    long n = 0;
	    try {
		begin();
		if (!is_open())
		    return 0;
		readerThread = NativeThread.current();
		for (;;) {
		    n = IOUtil.read(fd, bufs, nd);
		    if ((n == IOStatus.INTERRUPTED) && is_open())
			continue;
		    return IOStatus.normalize(n);
		}
	    } finally {
		readerThread = 0;
		end(n > 0 || (n == IOStatus.UNAVAILABLE));
		synchronized (stateLock) {
		    if ((n <= 0) && (!is_input_open))
			return IOStatus.EOF;
		}
		assert IOStatus.check(n);
	    }
	}




    }

	long DatagramChannel::read(ByteBuffer[] dsts, int offset, int length)
      {

        if ((offset < 0) || (length < 0) || (offset > dsts.length - length))
            throw new IndexOutOfBoundsException();
	// ## Fix IOUtil.write so that we can avoid this array copy
	return read0(Util.subsequence(dsts, offset, length));

             //////

    }
*/

int DatagramChannel::write(ByteBuffer& bb)
{
    ensureOpen();

    if (!is_open())
    {
        return -1;
    }

    for (;;)
    {
        int pos = bb.position();
        int lim = bb.limit();
        assert (pos <= lim);
        int rem = (pos <= lim ? lim - pos : 0);

        int written = 0;
        if (rem == 0)
        {
            return -1;
        }

        written = ::write(fd, bb.address() + pos, rem);

        if (written > 0)
        {
            bb.position(pos + written);
        }
        return written;
    }

}

int DatagramChannel::send(ByteBuffer& bb, SocketAddress& target)
{
    ensureOpen();

    if (!is_open())
    {
        return -1;
    }

    for (;;)
    {
        int pos = bb.position();
        int lim = bb.limit();
        assert (pos <= lim);
        int rem = (pos <= lim ? lim - pos : 0);

        int written = 0;
        if (rem == 0)
        {
            return -1;
        }

        InetSocketAddress& inetaddr = (InetSocketAddress&)target;
        struct sockaddr sa;
        unsigned int sa_len = sizeof(struct sockaddr_in);
        Net::InetAddressToSockaddr(inetaddr.get_address(),  inetaddr.get_port(), &sa, (int*)&sa_len);
        written = ::sendto(fd, bb.address() + pos, rem, 0, (struct sockaddr *)&sa, sa_len);

        if (written > 0)
        {
            bb.position(pos + written);
        }
        return written;
    }
}
int  DatagramChannel::receive(ByteBuffer& bb, SocketAddress& sa_in)
{
    if (!ensureOpen())
    {
        return -1;
    }

    if (!is_open())
    {
        return -1;
    }

    struct sockaddr sa;
    unsigned int sa_len = sizeof(struct sockaddr_in);

    for (;;)
    {
        int pos = bb.position();
        int lim = bb.limit();

        int rem = (pos <= lim ? lim - pos : 0);

        if (rem == 0)
        {
            return -1;
        }

        int n = 0;

        n = ::recvfrom(fd, bb.address() + pos, rem, 0, (struct sockaddr *)&sa, &sa_len);

        if (n > 0)
        {
            bb.position(pos + n);

            //InetSocketAddress& inetaddr =  (InetSocketAddress&)sa;
            //netaddr
        }
        return n;
    }

}


/*
    long DatagramChannel::write(ByteBuffer[] srcs) {
	return write(srcs, 0, srcs.length);
    }
    long DatagramChannel::write0(ByteBuffer[] bufs)
    {
        if (bufs == null)
            throw new NullPointerException();
	synchronized (writeLock) {
            ensure_write_open();
	    long n = 0;
	    try {
		begin();
		if (!is_open())
		    return 0;
		writerThread = NativeThread.current();
		for (;;) {
		    n = IOUtil.write(fd, bufs, nd);
		    if ((n == IOStatus.INTERRUPTED) && is_open())
			continue;
		    return IOStatus.normalize(n);
		}
	    } finally {
		writerThread = 0;
		end((n > 0) || (n == IOStatus.UNAVAILABLE));
		synchronized (stateLock) {
		    if ((n <= 0) && (!is_onput_open))
			throw new AsynchronousCloseException();
		}
		assert IOStatus.check(n);
	    }
	}
    }

    long DatagramChannel::write(ByteBuffer[] srcs, int offset, int length)
    {
        if ((offset < 0) || (length < 0) || (offset > srcs.length - length))
            throw new IndexOutOfBoundsException();
	// ## Fix IOUtil.write so that we can avoid this array copy
	return write0(Util.subsequence(srcs, offset, length));
    }

*/
void DatagramChannel::impl_configure_blocking(bool block)
{
    Net::configure_blocking(fd, block);
}
/*
	SocketOpts DatagramChannel::options()
	{
	synchronized (stateLock) {
	    if (options == null) {
		SocketOptsImpl.Dispatcher d
		    = new SocketOptsImpl.Dispatcher() {
			    int getInt(int opt) throws IOException {
				return Net.getIntOption(fd, opt);
			    }
			    void setInt(int opt, int arg)
				throws IOException
			    {
				Net.setIntOption(fd, opt, arg);
			    }
			};
		options = new SocketOptsImpl.IP.TCP(d);
	    }
	    return options;
	}
    }
*/
bool DatagramChannel::is_bound()
{
    //return Net.localPortNumber(fd) != 0;
    return false;
}

SocketAddress* DatagramChannel::local_address()
{
    if ((state == ST_CONNECTED) && (_local_address == NULL)) {
        // Socket was not bound before connecting,
        // so ask what the address turned out to be
        _local_address = Net::local_address(fd);
    }
    return _local_address;
}

SocketAddress* DatagramChannel::remote_address()
{
    return _remote_address;
}

int  DatagramChannel::bind(SocketAddress& local)
{
    ensureOpen();
    if (_local_address != NULL)
        throw   AlreadyBoundException();
    InetSocketAddress* isa = (InetSocketAddress*)&local;
    Net::bind(fd, isa->get_address(), isa->get_port());
    _local_address = (SocketAddress*)Net::local_address(fd);

    return 0;
}

bool DatagramChannel::is_connected()
{
    return (state == ST_CONNECTED);
}


void DatagramChannel::ensure_open_and_unconnected()
{
    if (!is_open())
        return ;//throw  new ClosedChannelException;
    if (state == ST_CONNECTED)
        return ;//throw  new AlreadyConnectedException;
}

bool DatagramChannel::connect(SocketAddress& sa)
{
    int trafficClass = 0;		// ## Pick up from options
    //int localPort = 0;


    ensure_open_and_unconnected();
    InetSocketAddress* isa = (InetSocketAddress*)(&sa);

    int n = 0;

    if (is_open())
    {

        for (;;)
        {
            n = Net::connect(fd,
                             isa->get_address(),
                             isa->get_port(),
                             trafficClass);
            if (  ((n == -1)&&(errno == EINTR))
                    && is_open())
                continue;
            break;
        }
    }


    _remote_address = isa;
    if (n == 0)
    {
        // Connection succeeded; disallow further
        // invocation
        state = ST_CONNECTED;
        return true;
    }
    return false;

}


// AbstractInterruptibleChannel synchronizes invocations of this method
// using AbstractInterruptibleChannel.closeLock, and also ensures that this
// method is only ever invoked once.  Before we get to this method, is_open
// (which is volatile) will have been set to false.
//
void DatagramChannel::impl_close_selectablechannel()
{
    // If this channel is not registered then it's safe to close the fd
    // immediately since we know at this point that no thread is
    // blocked in an I/O operation upon the channel and, since the
    // channel is marked closed, no thread will start another such
    // operation.  If this channel is registered then we don't close
    // the fd since it might be in use by a selector.  In that case
    // closing this channel caused its keys to be cancelled, so the
    // last selector to deregister a key for this channel will invoke
    // kill() to close the fd.
    //
    if (!is_registered())
        kill();
}

void DatagramChannel::kill()
{
    if (state == ST_KILLED)
        return;
    if (state == ST_UNINITIALIZED)
    {
        state = ST_KILLED;
        return;
    }

    Net::close(fd);
    state = ST_KILLED;
}

/**
 * Translates native poll revent ops into a ready operation ops
 */
bool DatagramChannel::translate_ready_ops(int ops, int initialOps,SelectionKey& sk)
{
    int intOps = sk.nio_interest_ops(); // Do this just once, it synchronizes
    int oldOps = sk.nio_ready_ops();
    int newOps = initialOps;

    if ((ops & Selector::OP_NVAL) != 0) {
        // This should only happen if this channel is pre-closed while a
        // selection operation is in progress
        // ## Throw an error if this channel has not been pre-closed
        return false;
    }

    if ((ops & (Selector::OP_ERR | Selector::OP_HUP)) != 0) {
        newOps = intOps;
        sk.nio_ready_ops(newOps);
        // No need to poll again in check_connect,
        // the error will be detected there

        return (newOps & ~oldOps) != 0;
    }

    if (((ops & Selector::OP_READ) != 0) &&
            ((intOps & SelectionKey::OP_READ) != 0) )
        newOps |= SelectionKey::OP_READ;


    if (((ops & Selector::OP_WRITE) != 0) &&
            ((intOps & SelectionKey::OP_WRITE) != 0) )
        newOps |= SelectionKey::OP_WRITE;

    sk.nio_ready_ops(newOps);
    return (newOps & ~oldOps) != 0;
}

bool DatagramChannel::translate_and_update_ready_ops(int ops, SelectionKey& sk)
{
    return translate_ready_ops(ops, sk.ready_ops(), sk);
}

bool DatagramChannel::translate_and_set_ready_ops(int ops, SelectionKey& sk)
{
    return translate_ready_ops(ops, 0, sk);
}

/**
 * Translates an interest operation set into a native poll event set
 */
bool  DatagramChannel::translate_and_set_interest_ops(int ops, SelectionKey& sk)
{
    int newOps = 0;
    if ((ops & SelectionKey::OP_READ) != 0)
        newOps |= Selector::OP_READ;
    if ((ops & SelectionKey::OP_WRITE) != 0)
        newOps |= Selector::OP_WRITE;
    if ((ops & SelectionKey::OP_CONNECT) != 0)
        newOps |= Selector::OP_READ;
    sk.selector()->put_event_ops(sk, newOps);
    return true;
}

int DatagramChannel::get_fd()
{
    return fd;
}





