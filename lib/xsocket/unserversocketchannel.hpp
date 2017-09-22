#ifndef _BASE_NET_UNSERVERSOCKETCHANNEL_H_
#define _BASE_NET_UNSERVERSOCKETCHANNEL_H_
#include "selectablechannel.hpp"
#include "socketaddress.hpp"

class UNSocketChannel;
class UNServerSocketChannel: public SelectableChannel
{
private:
	int fd;
	// Channel state, increases monotonically
	static  int ST_UNINITIALIZED;
	static  int ST_INUSE;
	static  int ST_KILLED;

	int state;

	// Binding
	SocketAddress* _localAddress; // null => unbound
	// Options, created on demand
	//SocketOpts.IP.TCP options;

public:
	UNServerSocketChannel(SelectorProvider* sp);
	UNServerSocketChannel(SelectorProvider* sp, int fd);

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
	 static UNServerSocketChannel* open();
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
	int validOps() ;
	bool isBound();
	SocketAddress* localAddress();
	int  bind(SocketAddress& local, int backlog);
	UNSocketChannel* accept();
	int accept(UNSocketChannel* sc);
	//SocketOpts options();
	void kill();
     
	bool translateReadyOps(int ops, int initialOps,SelectionKey& sk);

	bool translateAndUpdateReadyOps(int ops, SelectionKey& sk);
	bool translateAndSetReadyOps(int ops, SelectionKey& sk) ;

	/**
	 * Translates an interest operation set into a native poll event set
	 */
	bool translateAndSetInterestOps(int ops, SelectionKey& sk) ;
	int getFD();

	static  int listen(int fd, int backlog);

	void implConfigureBlocking(bool block);
	void implCloseSelectableChannel();

};

#endif

