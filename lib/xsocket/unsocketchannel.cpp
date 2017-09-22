#include <errno.h>
#include <assert.h>
#include <sys/poll.h>
#include <stdio.h>
#include <sys/un.h>
#include <new>

#include "net.hpp"
#include "selector.hpp"
#include "selectorprovider.hpp"
#include "selectionkey.hpp"
#include "unsocketchannel.hpp"

UNSocketChannel::UNSocketChannel()
        : SelectableChannel(0)
{
    _state = ST_UNINITIALIZED;
}

UNSocketChannel::UNSocketChannel(SelectorProvider* sp)
        : SelectableChannel(sp)
{
    _state = ST_UNINITIALIZED;

    _is_input_open = true;
    _is_output_open = true;
    _ready_to_connect = false;
    _fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    _state = ST_UNCONNECTED;

}

UNSocketChannel::~UNSocketChannel( )
{
    close();
}

int UNSocketChannel::open()
{
    if(_state == ST_UNINITIALIZED)
    {
        _is_input_open = true;
        _is_output_open = true;
        _ready_to_connect = false;
        _fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        _state = ST_UNCONNECTED;

        return 0;
    }

    return -1;
}

int UNSocketChannel::open(UNSocketAddress& remote)
{
    int rv = open();
    if(rv < 0)
    {
        return -1;
    }
    
    return connect(remote);
}

// Constructor for sockets obtained from server sockets

int UNSocketChannel::open(int fd)
{
    if(_state == ST_UNINITIALIZED)
    {
        _is_input_open = true;
        _is_output_open = true;
        _ready_to_connect = false;
        _fd = fd;
        _state = ST_UNCONNECTED;

        return 0;
    }

    return -1;
}

int UNSocketChannel::open(int fd, UNSocketAddress& remote)
{
    if(_state == ST_UNINITIALIZED)
    {
        _is_input_open = true;
        _is_output_open = true;
        _ready_to_connect = false;
        _fd = fd;
        _state = ST_UNCONNECTED;

        return connect(remote);
    }

    return -1;
}

int UNSocketChannel::valid_ops()
{
    return (SelectionKey::OP_READ | SelectionKey::OP_WRITE | SelectionKey::OP_CONNECT);
}


bool UNSocketChannel::ensure_read_open()
{
    if (!is_open())
        return false;
    if (!is_connected())
        return false;
    if (!is_input_open())
        return false;

    return true;
}

bool UNSocketChannel::ensure_write_open()
{
    if (!is_open())
        return false;
    if (!is_onput_open())
        return false;
    if (!is_connected())
        return false;
    return true;
}

int UNSocketChannel::read(ByteBuffer& bb)
{
    if (!ensure_read_open())
    {
        return -1;
    }

    if (!is_open())
    {
        return -1;
    }

    for (; ;)
    {
        int pos = bb.position();
        int lim = bb.limit();

        int rem = (pos <= lim ? lim - pos : 0);

        if (rem == 0)
        {
            return -1;
        }

        int n = 0;

        n = ::read(_fd, bb.address() + pos, rem);
        if ( (n == -1 ) && ((errno == EAGAIN) || ( errno == EINTR)) )
        {
            return 0;
        }
        if(n == 0)
        {

            return -1;
        }

        if (n > 0)
        {
            bb.position(pos + n);
        }

        return n;
    }
}

int UNSocketChannel::read(char* buf, int buf_len)
{
    if((buf == NULL) ||(buf_len <=0))
    {
        return -1;
    }

    int n = 0;

    n = ::read(_fd, buf, buf_len);
    if ( (n == -1 ) && ((errno == EAGAIN) || ( errno == EINTR)) )
    {
        return 0;
    }

    if(n == 0)
    {

        return -1;
    }

    return n;
}

int UNSocketChannel::write(ByteBuffer& bb)
{
    ensure_write_open();

    if (!is_open())
    {
        return -1;
    }

    for (; ;)
    {
        int pos = bb.position();
        int lim = bb.limit();
        assert(pos <= lim);
        int rem = (pos <= lim ? lim - pos : 0);

        int written = 0;
        if (rem == 0)
        {
            return -1;
        }

        written = ::write(_fd, bb.address() + pos, rem);
        if ( (written == -1 ) && ((errno == EAGAIN) || ( errno == EINTR)) )
        {
            return 0;
        }

        if (written > 0)
        {
            bb.position(pos + written);
        }
        return written;
    }
}

int UNSocketChannel::write(char* buf, int buf_len)
{
    if((buf == NULL) ||(buf_len <=0))
    {
        return -1;
    }

    int written = 0;

    written = ::write(_fd, buf, buf_len);
    if ( (written == -1 ) && ((errno == EAGAIN) || ( errno == EINTR)) )
    {
        return 0;
    }

    return written;
}


bool UNSocketChannel::is_bound()
{
    if (_state == ST_CONNECTED)
    {
        return true;
    }

    return _address.is_valid();
}

UNSocketAddress UNSocketChannel::address()
{
    if ((_state == ST_CONNECTED) && (!_address.is_valid()))
    {
        sockaddr sa;
        unsigned int sa_len = sizeof(sockaddr);
        // int port;
        if (getsockname(_fd, (struct sockaddr *)&sa, &sa_len) >= 0) 
        {
            sockaddr_un* sun = reinterpret_cast<sockaddr_un*>(&sa);
            new(&_address)UNSocketAddress(sun->sun_path);
        }
    }
    
    return _address;
}


int UNSocketChannel::bind(const char* un_sock_path)
{
    int rv;
    ensure_open_and_unconnected();
    if (_address.is_valid())
    {
        return -1;
    }

    struct sockaddr_un addr;

    strcpy(addr.sun_path, un_sock_path);
    addr.sun_family = AF_UNIX;
    rv = ::bind (_fd, (struct sockaddr *) &addr, strlen(addr.sun_path) + sizeof (addr.sun_family));

    if (rv < 0) 
    {
        return -1;
    }

    new(&_address)UNSocketAddress(un_sock_path);

    return 0;
}


bool UNSocketChannel::is_connected()
{
    return (_state == ST_CONNECTED);
}

bool UNSocketChannel::is_connection_pending()
{
    return (_state == ST_PENDING);
}

void UNSocketChannel::ensure_open_and_unconnected()
{
    if (!is_open())
        return ;
    if (_state == ST_CONNECTED)
        return ;
    if (_state == ST_PENDING)
        return ;
}

bool UNSocketChannel::connect(UNSocketAddress& sa)
{
    ensure_open_and_unconnected();

    int n = 0;

    if (is_open())
    {
        struct sockaddr_un server_addr;

        server_addr.sun_family = AF_UNIX;
        strcpy(server_addr.sun_path, sa.get_address());
    
        for (; ;)
        {
            n = ::connect(_fd, (struct sockaddr *)&server_addr, strlen(server_addr.sun_path) 
                                                                                    + sizeof (server_addr.sun_family));  
            if (((n == -1) && (errno == EINTR)) && is_open())
                continue;
            break;
        }
    }

    if (n == 0)
    {
        // Connection succeeded; disallow further
        // invocation
        _state = ST_CONNECTED;
        new(&_address)UNSocketAddress(sa.get_address());

        return true;
    }

    // If nonblocking and no exception then connection
    // pending; disallow another invocation
    if (!is_blocking())
        _state = ST_PENDING;
    else
        return false;

    return false;
}

bool UNSocketChannel::connect(const char* un_sock_path)
{
    UNSocketAddress sa(un_sock_path);
    return connect(sa);
}

bool UNSocketChannel::finish_connect()
{
    if (!is_open())
        return false;
    if (_state == ST_CONNECTED)
        return true;
    if (_state != ST_PENDING)
        return false;

    int n = 0;

    if (is_open())
    {
        if (!is_blocking())
        {
            for (; ;)
            {
                n = check_connect(_fd, false, _ready_to_connect);
                if (((n == -1) && (errno == EINTR)) && is_open())
                    continue;
                break;
            }
        }
        else
        {
            for (; ;)
            {
                n = check_connect(_fd, true, _ready_to_connect);
                if (n == 0)
                {
                    // Loop in case of
                    // spurious notifications
                    continue;
                }
                if (((n == -1) && (errno == EINTR)) && is_open())
                    continue;
                break;
            }
        }
    }

    if (n == 0)
    {
        _state = ST_CONNECTED;
        return true;
    }
    return false;
}

int UNSocketChannel::shutdown(int fd, int how)
{
    int rv = Net::shutdown(fd, how);
    return rv;
}

int UNSocketChannel::shutdown_input()
{
    if (!is_open())
        return 0;
    _is_input_open = false;
    return shutdown(_fd, SHUT_RD);
}

int UNSocketChannel::shutdown_output()
{
    if (!is_open())
        return 0;
    _is_output_open = false;
    return shutdown(_fd, SHUT_WR);
}

bool UNSocketChannel::is_input_open()
{
    return _is_input_open;
}

bool UNSocketChannel::is_onput_open()
{
    return _is_output_open;
}

void UNSocketChannel::kill()
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

int UNSocketChannel::check_connect(int fd, bool block, bool ready)
{
    int error = 0;
    unsigned int n = sizeof(int);
    int result = 0;
    struct pollfd poller;

    poller.revents = 1;
    if (!ready)
    {
        poller.fd = fd;
        poller.events = POLLOUT;
        poller.revents = 0;
        result = ::poll(&poller, 1, block ? -1 : 0);
        if (result < 0)
        {
            return result;
        }
        if (!block && (result == 0))
            return result;
    }

    if (poller.revents)
    {
        errno = 0;
        result = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &n);
        if (result < 0)
        {
            return result;
        }
        else if (error)
        {
            errno = error;
            return -1;
        }
        return 0;
    }
    return 0;
}


void UNSocketChannel::close()
{
    deregister();

    if (!is_registered())
    {
        _is_input_open = false;
        _is_output_open = false;

        kill();
    }

    SelectableChannel::close();

}

/**
 * Translates native poll revent ops into a ready operation ops
 */
bool UNSocketChannel::translate_ready_ops(int ops, int initialOps, SelectionKey& sk)
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
        // No need to poll again in check_connect,
        // the error will be detected there
        _ready_to_connect = true;
        return (newOps & ~oldOps) != 0;
    }

    if (((ops & Selector::OP_READ) != 0) && ((intOps & SelectionKey::OP_READ) != 0) && (_state == ST_CONNECTED))
        newOps |= SelectionKey::OP_READ;

    if (((ops & Selector::OP_WRITE) != 0) && ((intOps & SelectionKey::OP_CONNECT) != 0) && ((_state == ST_UNCONNECTED) || (_state == ST_PENDING)))
    {
        newOps |= SelectionKey::OP_CONNECT;
        _ready_to_connect = true;
    }

    if (((ops & Selector::OP_WRITE) != 0) && ((intOps & SelectionKey::OP_WRITE) != 0) && (_state == ST_CONNECTED))
        newOps |= SelectionKey::OP_WRITE;

    sk.nio_ready_ops(newOps);
    return (newOps & ~oldOps) != 0;
}

bool UNSocketChannel::translate_and_update_ready_ops(int ops, SelectionKey& sk)
{
    return translate_ready_ops(ops, sk.ready_ops(), sk);
}

bool UNSocketChannel::translate_and_set_ready_ops(int ops, SelectionKey& sk)
{
    return translate_ready_ops(ops, 0, sk);
}

/**
 * Translates an interest operation set into a native poll event set
 */

bool UNSocketChannel::translate_and_set_interest_ops(int ops, SelectionKey& sk)
{
    int newOps = 0;
    if ((ops & SelectionKey::OP_READ) != 0)
        newOps |= Selector::OP_READ;
    if ((ops & SelectionKey::OP_WRITE) != 0)
        newOps |= Selector::OP_WRITE;

    if ((ops & SelectionKey::OP_CONNECT) != 0)
        newOps |= Selector::OP_WRITE;
    sk.selector()->put_event_ops(sk, newOps);

    return true;
}




void UNSocketChannel::impl_configure_blocking(bool block)
{
    Net::configure_blocking(_fd, block);
}

