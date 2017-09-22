#ifndef _BASE_NET_SOCKETCHANNEL_H_
#define _BASE_NET_SOCKETCHANNEL_H_

#include <sys/time.h>
#include <sys/uio.h>
#include "socketaddress.hpp"
#include "selectablechannel.hpp"
#include "socketchannel.hpp"
#include "bytebuffer.hpp"
#include "bytechannel.hpp"
#include "net.hpp"

class SocketChannel : public SelectableChannel, public ByteChannel
{
public:
    //socket error no
    static int ERR_UNDEFINED;    
    static int ERR_NORMAL_CLOSED;

private:
    // State, increases monotonically
    static int SHUT_RD;
    static int SHUT_WR;
    static int SHUT_RDWR;

    static  int ST_UNINITIALIZED;
    static  int ST_UNCONNECTED;
    static  int ST_PENDING;
    static  int ST_CONNECTED;
    static  int ST_KILLED;


    int _fd;
    int                    _state;
    int                    _last_error;

    // Binding
    InetSocketAddress _local_address;
    InetSocketAddress _remote_address;

    // Input/Output open
    bool _is_input_open;
    bool _is_output_open;
    bool _ready_to_connect;

public:
    // Constructor for normal connecting sockets
    //
    SocketChannel();
    SocketChannel(SelectorProvider* sp);

    // Constructor for sockets obtained from server sockets
    //
    SocketChannel(SelectorProvider* sp, int fd, InetSocketAddress& remote);
    ~SocketChannel( );

    SocketChannel& operator=(const SocketChannel& rhs);

    static SocketChannel* open();
    static SocketChannel* open(InetSocketAddress& remote);
    static SocketChannel* open(char* ip, int port);
    int valid_ops();

    int create();
    int read(ByteBuffer& dst);
    int sync_read(ByteBuffer& dst, int timeout);
    int sync_read(char* buf, int buf_len, int timeout);
    int read(char* buf, int buf_len);

    int write(ByteBuffer& src);
	int sync_writev(const struct iovec* vec, int count, int timeout);
    int sync_write(char* buf, int buf_len, int timeout);
    int sync_write(ByteBuffer& src, int timeout);    
    int write(char* buf, int buf_len);

    bool ensure_read_open();
    bool ensure_write_open();

    bool is_bound() ;
    InetSocketAddress local_address();
    InetSocketAddress remote_address();

    int bind(InetSocketAddress& local) ;
    int bind(int port);
    int bind(char* ip, int port);

    bool is_connected();
    bool is_connection_pending();
    bool ensure_open_and_unconnected();
    bool connect(InetSocketAddress& sa);
    bool connect(InetSocketAddress& sa, int timeout);
    bool connect(char* ip, int port);
    bool connect(char* ip, int port, int timeout);    
    bool finish_connect();

    static  int check_connect(int fd, bool block, bool ready);
    static  int shutdown(int fd, int how);

    int shutdown_input();
    int shutdown_output();
    bool is_input_open();
    bool is_onput_open();

    void close();
    int get_fd();
    void kill();
    int   get_last_error();
    
    // socket options
    // SO_KEEPALIVE
    bool keep_alive();
    void keep_alive(bool b);

    // SO_LINGER
    int linger();
    void linger(int n);

    // SO_OOBINLINE
    bool out_of_band_inline();
    void out_of_band_inline(bool b);

    // SO_RCVBUF
    int recv_buf_size();
    void recv_buf_size(int n);

    // SO_SNDBUF
    int send_buf_size();
    void send_buf_size(int n);

    // SO_REUSEADDR
    bool reuse_addr() ;
    void reuse_addr(bool b);

    //  SO_SNDTIMEO
    int send_timeout();
    void  send_timeout(int seconds);
    
    bool tcp_nodelay();
    void tcp_nodelay(bool b);
    //
    /**
     * Translates native poll revent ops into a ready operation ops
     */
    bool translate_ready_ops(int ops, int initialOps, SelectionKey& sk) ;
    bool translate_and_update_ready_ops(int ops, SelectionKey& sk);
    bool translate_and_set_ready_ops(int ops, SelectionKey& sk);
    bool translate_and_set_interest_ops(int ops, SelectionKey& sk);


private:


    void impl_configure_blocking(bool block);
};


inline
SocketChannel& SocketChannel::operator=(const SocketChannel& rhs)
{
    *(SelectableChannel*)this = *(SelectableChannel*)&rhs;
    _fd = dup(rhs._fd);
	
    //assert(_fd>0);
    //dup maybe failed, so please take care of your ulimit setting
    
    _state = rhs._state;

    // Binding
     _local_address = rhs._local_address;
     _remote_address = rhs._remote_address;

     _is_input_open  = rhs._is_input_open;
     _is_output_open = rhs._is_output_open;
     _ready_to_connect = rhs._ready_to_connect;

    return *this;
}
    
inline
int SocketChannel::get_fd()
{
    return _fd;
}


inline
int SocketChannel::get_last_error()
{
    return _last_error;
}


// socket options
// SO_KEEPALIVE
inline
bool  SocketChannel::keep_alive()
{
    return Net::get_bool_option(_fd, SOL_SOCKET,  SO_KEEPALIVE);
}

inline
void  SocketChannel::keep_alive(bool b)
{
    Net::set_bool_option(_fd, SOL_SOCKET,  SO_KEEPALIVE, b);
}

// SO_LINGER
inline
int  SocketChannel::linger()
{
    return Net::get_int_option(_fd, SOL_SOCKET,  SO_LINGER);
}

inline
void  SocketChannel::linger(int n)
{
    Net::set_int_option(_fd, SOL_SOCKET,  SO_LINGER, n);
}

// SO_OOBINLINE
inline
bool  SocketChannel::out_of_band_inline()
{
    return Net::get_bool_option(_fd, SOL_SOCKET,  SO_OOBINLINE);
}

inline
void  SocketChannel::out_of_band_inline(bool b)
{
    Net::set_bool_option(_fd, SOL_SOCKET,  SO_OOBINLINE, b);
}

// SO_RCVBUF
inline
int  SocketChannel::recv_buf_size()
{
    return Net::get_int_option(_fd, SOL_SOCKET,  SO_RCVBUF);
}

inline
void  SocketChannel::recv_buf_size(int n)
{
    Net::set_int_option(_fd, SOL_SOCKET,  SO_RCVBUF, n);
}

// SO_SNDBUF
inline
int  SocketChannel::send_buf_size()
{
    return Net::get_int_option(_fd, SOL_SOCKET,  SO_SNDBUF);
}

inline
void  SocketChannel::send_buf_size(int n)
{
    Net::set_int_option(_fd, SOL_SOCKET,  SO_SNDBUF, n);
}

// SO_REUSEADDR
inline
bool  SocketChannel::reuse_addr()
{
    return Net::get_bool_option(_fd, SOL_SOCKET,  SO_REUSEADDR);
}

inline
void  SocketChannel::reuse_addr(bool b)
{
    Net::set_bool_option(_fd, SOL_SOCKET,  SO_REUSEADDR, b);
}

//  SO_SNDTIMEO
inline
int SocketChannel::send_timeout()
{
	struct timeval tv;

       socklen_t arglen =  sizeof(timeval);
	::getsockopt(_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, &arglen);

       return tv.tv_sec;
}

inline
void  SocketChannel::send_timeout(int seconds)
{
	struct timeval tv;

	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	::setsockopt(_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

// SO_REUSEADDR
inline
bool  SocketChannel::tcp_nodelay()
{
    return Net::get_bool_option(_fd, IPPROTO_TCP,  TCP_NODELAY);
}

inline
void  SocketChannel::tcp_nodelay(bool b)
{
    Net::set_bool_option(_fd, IPPROTO_TCP, TCP_NODELAY, b);
}


#endif

