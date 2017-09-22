#include <errno.h>
#include <assert.h>
#include <sys/poll.h>
#include <stdio.h>
#include "net.hpp"
#include "selector.hpp"
#include "selectorprovider.hpp"
#include "selectionkey.hpp"
#include "socketchannel.hpp"

int SocketChannel::ST_UNINITIALIZED = -1;
int SocketChannel::ST_UNCONNECTED = 0;
int SocketChannel::ST_PENDING = 1;
int SocketChannel::ST_CONNECTED = 2;
int SocketChannel::ST_KILLED = 3;

int SocketChannel::SHUT_RD = 0;
int SocketChannel::SHUT_WR = 1;
int SocketChannel::SHUT_RDWR = 2;

int SocketChannel::ERR_UNDEFINED = 0;  
int SocketChannel::ERR_NORMAL_CLOSED = 1;


SocketChannel::SocketChannel()
        : SelectableChannel(0)
{
    _state = ST_UNINITIALIZED;
    _last_error = ERR_UNDEFINED;
}

SocketChannel::SocketChannel(SelectorProvider* sp)
        : SelectableChannel(sp)
{
    _state = ST_UNINITIALIZED;
    _last_error = ERR_UNDEFINED;    

    _is_input_open = true;
    _is_output_open = true;
    _ready_to_connect = false;
    _fd = Net::socket(true);
    _state = ST_UNCONNECTED;

}

// Constructor for sockets obtained from server sockets

SocketChannel::SocketChannel(SelectorProvider* sp, int fd, InetSocketAddress& remote)
        : SelectableChannel(sp)
{
    _state = ST_UNINITIALIZED;
    _last_error = ERR_UNDEFINED;    

    _is_input_open = true;
    _is_output_open = true;
    _ready_to_connect = false;

    _state = ST_CONNECTED;
    _fd = fd;

    _remote_address = remote;
}

SocketChannel::~SocketChannel( )
{
    close();
}

SocketChannel* SocketChannel::open()
{
    return SelectorProvider::provider().openSocketChannel();
}

SocketChannel* SocketChannel::open(InetSocketAddress& remote)
{
    SocketChannel* sc = open();
    sc->connect(remote);
    return sc;
}

SocketChannel* SocketChannel::open(char* ip, int port)
{
    SocketChannel* sc = open();
    sc->connect(ip, port);
    return sc;
}

int SocketChannel::valid_ops()
{
    return (SelectionKey::OP_READ | SelectionKey::OP_WRITE | SelectionKey::OP_CONNECT);
}

int SocketChannel::create()
{
    if(_state == ST_UNINITIALIZED)
    {
        _is_input_open = true;
        _is_output_open = true;
        _ready_to_connect = false;
        _fd = Net::socket(true);
        _state = ST_UNCONNECTED;

        return 0;

    }

    return -1;
}

bool SocketChannel::ensure_read_open()
{
    if (!is_open())
        return false;
    if (!is_connected())
        return false;
    if (!is_input_open())
        return false;

    return true;
}

bool SocketChannel::ensure_write_open()
{
    if (!is_open())
        return false;
    if (!is_onput_open())
        return false;
    if (!is_connected())
        return false;
    return true;
}

int SocketChannel::read(ByteBuffer& bb)
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
            _last_error = ERR_UNDEFINED;
            return -1;
        }

        int n = 0;

        n = ::read(_fd, bb.address() + pos, rem);
        if (n == -1)
        {
            if ((errno == EAGAIN) || ( errno == EINTR))
            {
                return 0;
            }
            else
            {
                _last_error = ERR_UNDEFINED;
                return -1;
            }
        }
        if(n == 0)
        {
            _last_error = ERR_NORMAL_CLOSED;
            return -1;
        }

        if (n > 0)
        {
            bb.position(pos + n);
        }

        return n;
    }
}


int SocketChannel::sync_read(ByteBuffer& bb, int timeout)
{
    if (!ensure_read_open())
    {
        return -1;
    }

    if (!is_open())
    {
        return -1;
    }

    int rv = 0;
    int pre_pos = bb.position();
    int rem = 0;

    long long prev_time = Net::current_time_millis();
        
    while (((rem = bb.remaining()) > 0) && (timeout>0))
    {
        int pos = bb.position();
        int n = 0;
        long long new_time;

        struct pollfd pfd;
        pfd.fd = _fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        errno = 0;
        rv = ::poll(&pfd, 1, timeout);

        if (rv > 0) 
        {
            n = ::read(_fd, bb.address() + pos, rem);
            if ( (n == -1 ) && ((errno == EAGAIN) || ( errno == EINTR)) )
            {
                continue;
            }
            
            if(n == 0)
            {
                return -1;
            }

            if (n > 0)
            {
                bb.position(pos + n);
            }
        }

        if(rv ==0 )
        {
            return 0;
        }

        /* adjust timeout and restart
         */
        new_time = Net::current_time_millis();
        timeout -= (new_time - prev_time);
        prev_time = new_time;

    }

    if(!bb.hasRemaining())
    {
        return bb.position() - pre_pos;        
    }

    //else timeout
    return 0;
}

int SocketChannel::sync_read(char* buf, int buf_len, int timeout)
{
    if (!ensure_read_open())
    {
        return -1;
    }

    if (!is_open())
    {
        return -1;
    }

    int rv = 0;
    int readed_count = 0;
    int rem = buf_len;

    long long prev_time = Net::current_time_millis();
        
    while ((rem > 0) && (timeout>0))
    {
        int n = 0;
        long long new_time;

        struct pollfd pfd;
        pfd.fd = _fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        errno = 0;
        rv = ::poll(&pfd, 1, timeout);

        if (rv > 0) 
        {
            n = ::read(_fd, buf + readed_count, rem);
            if ( (n == -1 ) && ((errno == EAGAIN) || ( errno == EINTR)) )
            {
                continue;
            }
            
            if(n == 0)
            {
                return -1;
            }

            if (n > 0)
            {
                rem -= n;
				readed_count += n;
            }
        }

        if(rv == 0 )
        {
            return 0;
        }

        /* adjust timeout and restart
         */
        new_time = Net::current_time_millis();
        timeout -= (new_time - prev_time);
        prev_time = new_time;

    }

    if(rem == 0)
    {
        return readed_count;        
    }

    //else timeout
    return 0;

}

int SocketChannel::read(char* buf, int buf_len)
{
    if((buf == NULL) ||(buf_len <=0))
    {
        _last_error = ERR_UNDEFINED;
        return -1;
    }

    int n = 0;

    n = ::read(_fd, buf, buf_len);
    if (n == -1 ) 
    {
        if((errno == EAGAIN) || ( errno == EINTR)) 
        {
            return 0;
        }
        else
        {
            _last_error = ERR_UNDEFINED;
            return -1;
        }
    }

    if(n == 0)
    {
        _last_error = ERR_NORMAL_CLOSED;
        return -1;
    }

    return n;
}

int SocketChannel::write(ByteBuffer& bb)
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


int SocketChannel::write(char* buf, int buf_len)
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

int SocketChannel::sync_writev(const struct iovec* vec, int count, int timeout)
{
	ensure_write_open();

	if (!is_open())
	{
	    return -1;
	}

	int rv = 0;
	int rem = 0;
	struct iovec real_vec[count];

	for (int i = 0; i < count; i++)
	{
		real_vec[i].iov_base = (vec + i)->iov_base;
		real_vec[i].iov_len = (vec + i)->iov_len;
		rem += (vec + i)->iov_len;
	}

	int write_count = 0; //已写字节数

	long long prev_time = Net::current_time_millis();
	    
	while ((rem > 0) && (timeout > 0))
	{
	    int n = 0;
	    long long new_time;

	    struct pollfd pfd;
	    pfd.fd = _fd;
	    pfd.events = POLLOUT;
	    pfd.revents = 0;
	    errno = 0;
	    rv = ::poll(&pfd, 1, timeout);

//		modify real_vec
 		int next_idx = 0;
		int tmp_total = 0;
		for (int i = 0; i < count; i++)
		{
			tmp_total += real_vec[i].iov_len;
			if (tmp_total > write_count)
			{
				next_idx = i;
				break;
			}
		}

		real_vec[next_idx].iov_base = (char*)real_vec[next_idx].iov_base + real_vec[next_idx].iov_len - (tmp_total - write_count);
		real_vec[next_idx].iov_len = tmp_total - write_count;

	    if (rv > 0) 
	    {
	        n = ::writev(_fd, &real_vec[next_idx], count - next_idx);
	        if ( (n == -1 ) && ((errno == EAGAIN) || ( errno == EINTR)) )
	        {
	            continue;
	        }

	        if(n == -1)
	        {
	            return -1;
	        }

	        if (n > 0)
	        {
	            write_count += n;
				rem -= n;
	        }
	    }

	    if(rv == 0 )
	    {
	        return 0;
	    }

	    /* adjust timeout and restart
	     */
	    new_time = Net::current_time_millis();
	    timeout -= (new_time - prev_time);
	    prev_time = new_time;

	}

	if(rem == 0)
	{
	    return write_count;        
	}

	//else timeout
	return 0;
}


int SocketChannel::sync_write(ByteBuffer& bb, int timeout)
{
    ensure_write_open();

    if (!is_open())
    {
        return -1;
    }
    
    int rv = 0;
    int pre_pos = bb.position();
    int rem = 0;

    long long prev_time = Net::current_time_millis();
        
    while (((rem = bb.remaining()) > 0) && (timeout>0))
    {
        int pos = bb.position();
        int n = 0;
        long long new_time;

        struct pollfd pfd;
        pfd.fd = _fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        errno = 0;
        rv = ::poll(&pfd, 1, timeout);

        if (rv > 0) 
        {
            n = ::write(_fd, bb.address() + pos, rem);
            if ( (n == -1 ) && ((errno == EAGAIN) || ( errno == EINTR)) )
            {
                continue;
            }

            if(n== -1)
            {
                return -1;
            }

            if (n > 0)
            {
                bb.position(pos + n);
            }
        }

        if(rv ==0 )
        {
            return 0;
        }

        /* adjust timeout and restart
         */
        new_time = Net::current_time_millis();
        timeout -= (new_time - prev_time);
        prev_time = new_time;

    }

    if(!bb.hasRemaining())
    {
        return bb.position() - pre_pos;        
    }

    //else timeout
    return 0;
}

int SocketChannel::sync_write(char* buf, int buf_len, int timeout)
{
	ensure_write_open();

	if (!is_open())
	{
	    return -1;
	}

	int rv = 0;

	int rem = buf_len;
	int write_count = 0; //已写字节数

	long long prev_time = Net::current_time_millis();
	    
	while ((rem > 0) && (timeout > 0))
	{
	    int n = 0;
	    long long new_time;

	    struct pollfd pfd;
	    pfd.fd = _fd;
	    pfd.events = POLLOUT;
	    pfd.revents = 0;
	    errno = 0;
	    rv = ::poll(&pfd, 1, timeout);

	    if (rv > 0) 
	    {
	        n = ::write(_fd, buf + write_count, rem);
	        if ( (n == -1 ) && ((errno == EAGAIN) || ( errno == EINTR)) )
	        {
	            continue;
	        }

	        if(n == -1)
	        {
	            return -1;
	        }

	        if (n > 0)
	        {
	            write_count += n;
				rem -= n;
	        }
	    }

	    if(rv == 0 )
	    {
	        return 0;
	    }

	    /* adjust timeout and restart
	     */
	    new_time = Net::current_time_millis();
	    timeout -= (new_time - prev_time);
	    prev_time = new_time;

	}

	if(rem == 0)
	{
	    return write_count;        
	}

	//else timeout
	return 0;
}


bool SocketChannel::is_bound()
{
    if (_state == ST_CONNECTED)
    {
        return true;
    }

    return _local_address.is_valid();
}

InetSocketAddress SocketChannel::local_address()
{
    if ((_state == ST_CONNECTED) && (!_local_address.is_valid()))
    {
        // Socket was not bound before connecting,
        // so ask what the address turned out to be
        _local_address = Net::local_address(_fd);
    }
    return _local_address;
}

InetSocketAddress SocketChannel::remote_address()
{
    return _remote_address;
}

int SocketChannel::bind(InetSocketAddress& local)
{
    ensure_open_and_unconnected();
    if (_local_address.is_valid())
    {
        return -1;
    }

    Net::bind(_fd, local.get_address(), local.get_port());
    _local_address = Net::local_address(_fd);

    return 0;
}

int SocketChannel::bind(int port)
{
    InetSocketAddress sa(port);
    return bind(sa);
}

int SocketChannel::bind(char* ip, int port)
{
    InetSocketAddress sa(ip, port);
    return bind(sa);
}

bool SocketChannel::is_connected()
{
    return (_state == ST_CONNECTED);
}

bool SocketChannel::is_connection_pending()
{
    return (_state == ST_PENDING);
}

bool SocketChannel::ensure_open_and_unconnected()
{
    if (!is_open())
        return false;
    if (_state == ST_CONNECTED)
        return false;
    if (_state == ST_PENDING)
        return false;

    return true;
}

bool SocketChannel::connect(InetSocketAddress& sa)
{
    int trafficClass = 0;

    if(!ensure_open_and_unconnected())
    {
        return false;
    }

    int n = 0;

    for (; ;)
    {
        n = Net::connect(_fd, sa.get_address(), sa.get_port(), trafficClass);
        if (((n == -1) && (errno == EINTR)) && is_open())
            continue;
        break;
    }

    if (n == 0)
    {
        // Connection succeeded; disallow further
        // invocation
        _state = ST_CONNECTED;
        _remote_address = sa;

        return true;
    }

    // If nonblocking and no exception then connection
    // pending; disallow another invocation
    if ((!is_blocking()) && (errno == EINPROGRESS))
    {
        _state = ST_PENDING;
    }

    return false;
}

bool SocketChannel::connect(char* ip, int port)
{
    InetSocketAddress sa(ip, port);
    return connect(sa);
}

bool SocketChannel::finish_connect()
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


bool SocketChannel::connect(InetSocketAddress& sa, int timeout)
{
    int connect_rv = 0;
    if (timeout <= 0) 
    {
        return connect(sa);
    } 

    /* 
     * A timeout was specified. We put the socket into non-blocking
     * mode, connect, and then wait for the connection to be 
     * established, fail, or timeout.
     */
    if(!ensure_open_and_unconnected())
    {
        return false;
    }
    
    Net::configure_blocking(_fd, false);
    connect_rv = Net::connect(_fd, sa.get_address(), sa.get_port(), 0);

    /* connection established immediately */
    if (connect_rv == 0) 
    {
        return true;
    }
    
    unsigned int optlen;
    long long prev_time = Net::current_time_millis();

    if (errno != EINPROGRESS) 
    {
        Net::configure_blocking(_fd, true);
        return false;
    }

    /*
    * Wait for the connection to be established or a
    * timeout occurs. poll/select needs to handle EINTR in
    * case lwp sig handler redirects any process signals to
    * this thread.
    */
    while (1) 
    {
        long long new_time;

        struct pollfd pfd;
        pfd.fd = _fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        errno = 0;
        connect_rv = ::poll(&pfd, 1, timeout);

        if (connect_rv >= 0) 
        {
            break;
        }

        if (errno != EINTR) 
        {
            break;
        }

        /*
         * The poll was interrupted so adjust timeout and
         * restart
         */
        new_time = Net::current_time_millis();
        timeout -= (new_time - prev_time);
        if (timeout <= 0) 
        {
            connect_rv = 0;
            break;
        }
        prev_time = new_time;

    } /* while */

    if (connect_rv == 0) 
    {
        /*
        * Timeout out but connection may still be established.
        * At the high level it should be closed immediately but
        * just in case we make the socket blocking again and
        * shutdown input & output.
        */
        Net::configure_blocking(_fd, true);
        errno = ETIMEDOUT;
        return false;
    }

    /* has connection been established */

    errno = 0;
    optlen = sizeof(connect_rv);
    if(getsockopt(_fd, SOL_SOCKET, SO_ERROR, (void*)&connect_rv, &optlen) < 0)
    {
        return false;
    }
    else if (connect_rv)
    {
        errno = connect_rv;
        return false;
    }

    _state = ST_CONNECTED;
    _remote_address = sa;

    Net::configure_blocking(_fd, true);
    return true;

}

bool SocketChannel::connect(char* ip, int port, int timeout)
{
    InetSocketAddress sa(ip, port);
    return connect(sa, timeout);
}

int SocketChannel::shutdown(int fd, int how)
{
    int rv = Net::shutdown(fd, how);
    return rv;
}

int SocketChannel::shutdown_input()
{
    if (!is_open())
        return 0;
    _is_input_open = false;
    return shutdown(_fd, SHUT_RD);
}

int SocketChannel::shutdown_output()
{
    if (!is_open())
    {
        return 0;
    }
    _is_output_open = false;
    return shutdown(_fd, SHUT_WR);
}

bool SocketChannel::is_input_open()
{
    return _is_input_open;
}

bool SocketChannel::is_onput_open()
{
    return _is_output_open;
}

void SocketChannel::kill()
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

int SocketChannel::check_connect(int fd, bool block, bool ready)
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


void SocketChannel::close()
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
bool SocketChannel::translate_ready_ops(int ops, int initialOps, SelectionKey& sk)
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

bool SocketChannel::translate_and_update_ready_ops(int ops, SelectionKey& sk)
{
    return translate_ready_ops(ops, sk.ready_ops(), sk);
}

bool SocketChannel::translate_and_set_ready_ops(int ops, SelectionKey& sk)
{
    return translate_ready_ops(ops, 0, sk);
}

/**
 * Translates an interest operation set into a native poll event set
 */

bool SocketChannel::translate_and_set_interest_ops(int ops, SelectionKey& sk)
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




void SocketChannel::impl_configure_blocking(bool block)
{
    Net::configure_blocking(_fd, block);
}

