#include <errno.h>
#include <new>
#include <string.h>
#include "ioexception.hpp"
#include "net.hpp"
#include "pollarraywrapper.hpp"
#include "selector.hpp"
#include "selectorprovider.hpp"
#include "selectionkey.hpp"
#include "socketchannel.hpp"
#include "serversocketchannel.hpp"
#include "datagramchannel.hpp"


// Channel state, increases monotonically
int ServerSocketChannel::ST_UNINITIALIZED = -1;
int ServerSocketChannel::ST_INUSE = 0;
int ServerSocketChannel::ST_KILLED = 1;


ServerSocketChannel::ServerSocketChannel(SelectorProvider* sp)
        : SelectableChannel(sp)
{
    _state = ST_UNINITIALIZED;

    _fd = Net::serverSocket(true);
    _state = ST_INUSE;
}

ServerSocketChannel::ServerSocketChannel(SelectorProvider* sp, int fd)
        : SelectableChannel(sp)
{
    _state = ST_UNINITIALIZED;

    _fd = fd;
    _state = ST_INUSE;
    _local_address = Net::local_address(_fd);
}


ServerSocketChannel* ServerSocketChannel::open()
{
    return  SelectorProvider::provider().openServerSocketChannel();
}

int ServerSocketChannel::valid_ops()
{
    return SelectionKey::OP_ACCEPT;
}

bool ServerSocketChannel::is_bound()
{
    return _local_address.is_valid();
}

InetSocketAddress ServerSocketChannel::local_address()
{
    return _local_address;
}

int ServerSocketChannel::bind(InetSocketAddress& local, int backlog)
{
    int rv;
    if (!is_open())
        return -1;
    if (is_bound())
        return -1;

    rv = Net::bind(_fd, local.get_address(), local.get_port());
    if (rv < 0)
    {
        return rv;
    }

    rv = listen(_fd, backlog < 1 ? 50 : backlog);
    if (rv < 0)
    {
        return rv;
    }

    _local_address = Net::local_address(_fd);

    return rv;
}

int ServerSocketChannel::bind(int port, int backlog)
{
    InetSocketAddress sa(port);
    return bind(sa, backlog);
}

int  ServerSocketChannel::bind(char* ip, int port, int backlog)
{
    InetSocketAddress sa(ip, port);
    return bind(sa, backlog);
}

SocketChannel* ServerSocketChannel::accept()
{
    if (!is_open())
    {
        return NULL;
    }
    if (!is_bound())
    {
        return NULL;
    }

    int n = 0;
    int newfd;
    InetSocketAddress sa;

    for (; ;)
    {
        n = Net::accept(this->_fd, newfd, sa);
        if (((n == -1) && (errno == EINTR)) && is_open())
        {
            continue;
        }
        break;
    }

    if (n == -1)
    {
        return NULL;
    }

    Net::configure_blocking(newfd, true);

    SocketChannel* sc = new  SocketChannel(provider(), newfd, sa);

    return sc;

}

int ServerSocketChannel::accept(SocketChannel& sc)
{
    if (!is_open())
        return -1;
    if (!is_bound())
        return -1;

    int n = 0;
    int newfd;
    InetSocketAddress sa;

    if (!is_open())
        return -1;

    for (; ;)
    {
        n = Net::accept(this->_fd, newfd, sa);
        if (((n == -1) && (errno == EINTR)) && is_open())
            continue;
        break;
    }

    if (n == -1)
        return -1;

    Net::configure_blocking(newfd, true);

    SocketChannel* sc_p = &sc;
    new(sc_p)SocketChannel(provider(), newfd, sa);

    return 0;
}

void ServerSocketChannel::kill()
{
    if (_state == ST_KILLED)
        return;
    if (_state == ST_UNINITIALIZED)
    {
        _state = ST_KILLED;
        return;
    }
    Net::close(_fd);
    _state = ST_KILLED;
}

int ServerSocketChannel::listen(int fd, int backlog)
{
    return Net::listen(fd, backlog);
}


bool ServerSocketChannel::translate_ready_ops(int ops, int initialOps, SelectionKey& sk)
{
    int intOps = sk.nio_interest_ops(); // Do this just once, it synchronizes
    int oldOps = sk.nio_ready_ops();
    int newOps = initialOps;

    if ((ops & Selector::OP_NVAL) != 0)
    {
        // This should only happen if this channel is pre-closed while a
        // selection operation is in progress
        // ## Throw an error if this channel has not been pre-closed
        return false;
    }

    if ((ops & (Selector::OP_ERR | Selector::OP_HUP)) != 0)
    {
        newOps = intOps;
        sk.nio_ready_ops(newOps);
        return (newOps & ~oldOps) != 0;
    }

    if (((ops & Selector::OP_READ) != 0) && ((intOps & SelectionKey::OP_ACCEPT) != 0))
        newOps |= SelectionKey::OP_ACCEPT;

    sk.nio_ready_ops(newOps);
    return (newOps & ~oldOps) != 0;
}

bool ServerSocketChannel::translate_and_update_ready_ops(int ops, SelectionKey& sk)
{
    return translate_ready_ops(ops, sk.ready_ops(), sk);
}

bool ServerSocketChannel::translate_and_set_ready_ops(int ops, SelectionKey& sk)
{
    return translate_ready_ops(ops, 0, sk);
}

/**
 * Translates an interest operation set into a native poll event set
 */
bool ServerSocketChannel::translate_and_set_interest_ops(int ops, SelectionKey& sk)
{
    int newOps = 0;

    // Translate ops
    if ((ops & SelectionKey::OP_ACCEPT) != 0)
        newOps |= Selector::OP_READ;

    sk.selector()->put_event_ops(sk, newOps);

    return true;
}

void ServerSocketChannel::impl_configure_blocking(bool block)
{
    Net::configure_blocking(_fd, block);
}

void ServerSocketChannel::close()
{
    deregister();

    if (!is_registered())
    {
        kill();
    }

    SelectableChannel::close();
}



