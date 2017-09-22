#include <errno.h>
#include <new>
#include "ioexception.hpp"
#include "net.hpp"
#include "pollarraywrapper.hpp"
#include "selector.hpp"
#include "selectorprovider.hpp"
#include "selectionkey.hpp"
#include "unsocketchannel.hpp"
#include "unserversocketchannel.hpp"

    // Channel state, increases monotonically
int   UNServerSocketChannel::ST_UNINITIALIZED = -1;
int   UNServerSocketChannel::ST_INUSE = 0;
int   UNServerSocketChannel::ST_KILLED = 1;


UNServerSocketChannel::UNServerSocketChannel(SelectorProvider* sp)
:SelectableChannel(sp)
{	
	state = ST_UNINITIALIZED;  
	_localAddress = NULL; 
	//options = NULL;
	
	this->fd =  Net::unserverSocket(true);
	this->state = ST_INUSE;
}

UNServerSocketChannel::UNServerSocketChannel(SelectorProvider* sp, int fd) 
:SelectableChannel(sp)	
{	
	state = ST_UNINITIALIZED;  
	//options = NULL;

	this->fd =  fd;
	this->state = ST_INUSE;
	//_localAddress = (SocketAddress*)Net::localAddress(fd);
}

/**
 * Opens a server-socket channel.
 *
 * <p> The new channel is created by invoking the {@link
 * java.nio.channels.spi.SelectorProvider#openUNServerSocketChannel
 * openUNServerSocketChannel} method of the system-wide default {@link
 * java.nio.channels.spi.SelectorProvider} object.
 *
 * <p> The new channel's socket is initially unbound; it must be bound to a
 * specific address via one of its socket's {@link
 * java.net.ServerSocket#bind(SocketAddress) bind} methods before
 * connections can be accepted.  </p>
 *
 * @return  A new socket channel
 *
 * @throws  IOException
 *          If an I/O error occurs
 */
UNServerSocketChannel* UNServerSocketChannel::open()
{
	return  SelectorProvider::provider().openUNServerSocketChannel();
}
    /**
     * Returns an operation set identifying this channel's supported
     * operations.
     *
     * <p> Server-socket channels only support the accepting of new
     * connections, so this method returns {@link SelectionKey#OP_ACCEPT}.
     * </p>
     *
     * @return  The valid-operation set
     */
int UNServerSocketChannel::validOps() 
{
	return SelectionKey::OP_ACCEPT;
}

bool UNServerSocketChannel::isBound() 
{
	    return _localAddress != NULL;
}

SocketAddress* UNServerSocketChannel::localAddress() 
{
	    return _localAddress;
}

int UNServerSocketChannel::bind(SocketAddress& local, int backlog)
{
       int rv;
	if (!isOpen())
		return -1;//throw  new ClosedChannelException();
	if (isBound())
		return -1;//throw  new AlreadyBoundException();
	
	UNSocketAddress* isa = (UNSocketAddress*)&local;
	
	rv= Net::unbind(fd, isa->getUN_Path());
	if(rv < 0)
	{
		return rv;
	}

	rv = listen(fd, backlog < 1 ? 50 : backlog);
	if(rv < 0)
	{
		return rv;
	}		

	//_localAddress =(SocketAddress*) Net::localAddress(fd);

	return rv;
	
}

UNSocketChannel* UNServerSocketChannel::accept()
{
	if (!isOpen())
		return NULL;//throw  new ClosedChannelException;
	if (!isBound())
		return NULL;//throw  new NotYetBoundException;

	int n = 0;
	int newfd;
	UNSocketAddress* isaa = new UNSocketAddress;

	if (!isOpen())
	    return NULL;

	for (;;) 
	{
	    n = Net::unaccept(this->fd, newfd, *isaa);
	    if (((n == -1)&&(errno == EINTR)) && isOpen())
		continue;
	    break;
	}

	if (n == -1)
		return NULL;

	Net::configureBlocking(newfd, true);
	UNSocketAddress isa = isaa[0];
	UNSocketChannel*  sc = new UNSocketChannel(provider(), newfd, isa);

	return sc;
}

int  UNServerSocketChannel::accept(UNSocketChannel* sc)
{
	if (!isOpen())
		return -1;//throw new ClosedChannelException;
	if (!isBound())
		return -1;//throw  new NotYetBoundException;

	int n = 0;
	int newfd;
	UNSocketAddress* isaa = new UNSocketAddress[1];

	if (!isOpen())
	    return -1;

	for (;;) 
	{
	    n = Net::unaccept(this->fd, newfd, *isaa);
	    if (((n == -1)&&(errno == EINTR)) && isOpen())
		continue;
	    break;
	}

	if (n == -1)
		return -1;

	Net::configureBlocking(newfd, true);
	UNSocketAddress isa = isaa[0];
	new(sc)UNSocketChannel(provider(), newfd, isa);

	return 0;
}
/*
SocketOpts UNServerSocketChannel::options() 
{
    if (options == null) {
	SocketOptsImpl.Dispatcher d
	    = new SocketOptsImpl.Dispatcher() {
		    int getInt(int opt) throws IOException {
			return Net.getIntOption(fd, opt);
		    }
		    void setInt(int opt, int arg) throws IOException {
			Net.setIntOption(fd, opt, arg);
		    }
		};
	options = new SocketOptsImpl.IP.TCP(d);
    }
    return options;
}
*/
void UNServerSocketChannel::kill() 
{
    if (state == ST_KILLED)
	return;
    if (state == ST_UNINITIALIZED) {
            state = ST_KILLED;
	return;
        }
    Net::close(fd);
    state = ST_KILLED;
}


bool UNServerSocketChannel::translateReadyOps(int ops, int initialOps,SelectionKey& sk) 
{
        int intOps = sk.nioInterestOps(); // Do this just once, it synchronizes
        int oldOps = sk.nioReadyOps();
        int newOps = initialOps;

        if ((ops & Selector::OP_NVAL) != 0) {
	    // This should only happen if this channel is pre-closed while a
	    // selection operation is in progress
	    // ## Throw an error if this channel has not been pre-closed
	    return false;
	}

        if ((ops & (Selector::OP_ERR
                    | Selector::OP_HUP)) != 0) {
            newOps = intOps;
            sk.nioReadyOps(newOps);
            return (newOps & ~oldOps) != 0;
        }

        if (((ops & Selector::OP_READ) != 0) &&
            ((intOps & SelectionKey::OP_ACCEPT) != 0))
                newOps |= SelectionKey::OP_ACCEPT;

        sk.nioReadyOps(newOps);
        return (newOps & ~oldOps) != 0;
}

bool UNServerSocketChannel::translateAndUpdateReadyOps(int ops, SelectionKey& sk) 
{
    return translateReadyOps(ops, sk.readyOps(), sk);
}

bool UNServerSocketChannel::translateAndSetReadyOps(int ops, SelectionKey& sk) 
{
    return translateReadyOps(ops, 0, sk);
}

    /**
     * Translates an interest operation set into a native poll event set
     */
bool UNServerSocketChannel::translateAndSetInterestOps(int ops, SelectionKey& sk) 
{
    int newOps = 0;

    // Translate ops
    if ((ops & SelectionKey::OP_ACCEPT) != 0)
        newOps |= POLLIN;
    // Place ops into pollfd array
    sk.selector()->putEventOps(sk, newOps);

    return true;	
}

int UNServerSocketChannel::getFD() 
{
    return fd;
}

int UNServerSocketChannel::listen(int fd, int backlog)
{
	return Net::listen( fd, backlog);
}

void UNServerSocketChannel::implConfigureBlocking(bool block)
{
	Net::configureBlocking(fd, block);
}

void UNServerSocketChannel::implCloseSelectableChannel()
{
	    if (!isRegistered())
		kill();
}



