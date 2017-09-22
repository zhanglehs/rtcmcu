#ifndef _BASE_NET_SERVERSOCKETCHANNEL_H_
#define _BASE_NET_SERVERSOCKETCHANNEL_H_
#include "selectablechannel.hpp"
#include "socketaddress.hpp"
#include "net.hpp"

class SocketChannel;
class ServerSocketChannel: public SelectableChannel
{
private:
    int _fd;
    // Channel state, increases monotonically
    static  int ST_UNINITIALIZED;
    static  int ST_INUSE;
    static  int ST_KILLED;

    int _state;

    // Binding
    InetSocketAddress _local_address; // null => unbound

public:
    enum
    {
        DEF_BACKLOG_LENGTH = 50,
    };

    ServerSocketChannel(SelectorProvider* sp);
    ServerSocketChannel(SelectorProvider* sp, int fd);
    virtual ~ServerSocketChannel(){}

    static ServerSocketChannel* open();

    int valid_ops() ;
    bool is_bound();
    InetSocketAddress local_address();
    int  bind(InetSocketAddress& local, int backlog = DEF_BACKLOG_LENGTH);
    int  bind(int port, int backlog = DEF_BACKLOG_LENGTH);
    int  bind(char* ip, int port, int backlog = DEF_BACKLOG_LENGTH);
    SocketChannel* accept();
    int accept(SocketChannel& sc);

    void close();
    void kill();

    int get_fd();

    static  int listen(int fd, int backlog);

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
    
    bool translate_ready_ops(int ops, int initialOps,SelectionKey& sk);

    bool translate_and_update_ready_ops(int ops, SelectionKey& sk);
    bool translate_and_set_ready_ops(int ops, SelectionKey& sk) ;

    /**
     * Translates an interest operation set into a native poll event set
     */
    bool translate_and_set_interest_ops(int ops, SelectionKey& sk) ;

    void impl_configure_blocking(bool block);

};

inline
int ServerSocketChannel::get_fd()
{
    return _fd;
}


// socket options
// SO_KEEPALIVE
inline
bool  ServerSocketChannel::keep_alive()
{
    return Net::get_bool_option(_fd, SOL_SOCKET,  SO_KEEPALIVE);
}

inline
void  ServerSocketChannel::keep_alive(bool b)
{
    Net::set_bool_option(_fd, SOL_SOCKET,  SO_KEEPALIVE, b);
}

// SO_LINGER
inline
int  ServerSocketChannel::linger()
{
    return Net::get_int_option(_fd, SOL_SOCKET,  SO_LINGER);
}

inline
void  ServerSocketChannel::linger(int n)
{
    Net::set_int_option(_fd, SOL_SOCKET,  SO_LINGER, n);
}

// SO_OOBINLINE
inline
bool  ServerSocketChannel::out_of_band_inline()
{
    return Net::get_bool_option(_fd, SOL_SOCKET,  SO_OOBINLINE);
}

inline
void  ServerSocketChannel::out_of_band_inline(bool b)
{
    Net::set_bool_option(_fd, SOL_SOCKET,  SO_OOBINLINE, b);
}

// SO_RCVBUF
inline
int  ServerSocketChannel::recv_buf_size()
{
    return Net::get_int_option(_fd, SOL_SOCKET,  SO_RCVBUF);
}

inline
void  ServerSocketChannel::recv_buf_size(int n)
{
    Net::set_int_option(_fd, SOL_SOCKET,  SO_RCVBUF, n);
}

// SO_SNDBUF
inline
int  ServerSocketChannel::send_buf_size()
{
    return Net::get_int_option(_fd, SOL_SOCKET,  SO_SNDBUF);
}

inline
void  ServerSocketChannel::send_buf_size(int n)
{
    Net::set_int_option(_fd, SOL_SOCKET,  SO_SNDBUF, n);
}

// SO_REUSEADDR
inline
bool  ServerSocketChannel::reuse_addr()
{
    return Net::get_bool_option(_fd, SOL_SOCKET,  SO_REUSEADDR);
}

inline
void  ServerSocketChannel::reuse_addr(bool b)
{
    Net::set_bool_option(_fd, SOL_SOCKET,  SO_REUSEADDR, b);
}

//  SO_SNDTIMEO
inline
int ServerSocketChannel::send_timeout()
{
	struct timeval tv;

       socklen_t arglen =  sizeof(timeval);
	::getsockopt(_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, &arglen);

       return tv.tv_sec;
}

inline
void  ServerSocketChannel::send_timeout(int seconds)
{
	struct timeval tv;

	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	::setsockopt(_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

// SO_REUSEADDR
inline
bool  ServerSocketChannel::tcp_nodelay()
{
    return Net::get_bool_option(_fd, IPPROTO_TCP,  TCP_NODELAY);
}

inline
void  ServerSocketChannel::tcp_nodelay(bool b)
{
    Net::set_bool_option(_fd, IPPROTO_TCP, TCP_NODELAY, b);
}
#endif

