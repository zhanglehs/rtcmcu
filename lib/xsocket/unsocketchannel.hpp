#ifndef _BASE_NET_UNSOCKETCHANNEL_H_
#define _BASE_NET_UNSOCKETCHANNEL_H_

#include "socketaddress.hpp"
#include "selectablechannel.hpp"
#include "bytebuffer.hpp"
#include "bytechannel.hpp"
#include "net.hpp"

class UNSocketChannel : public SelectableChannel, public ByteChannel
{
private:
    // State, increases monotonically
    static const int ST_UNINITIALIZED = -1;
    static const int ST_UNCONNECTED = 0;
    static const int ST_PENDING = 1;
    static const int ST_CONNECTED = 2;
    static const int ST_KILLED = 3;

    static const int SHUT_RD = 0;
    static const int SHUT_WR = 1;
    static const int SHUT_RDWR = 2;

    int          _fd;
    int          _state;

    // Binding
    UNSocketAddress  _address;

    // Input/Output open
    bool _is_input_open;
    bool _is_output_open;
    bool _ready_to_connect;

public:
    // Constructor for normal connecting sockets
    //
    UNSocketChannel();
    UNSocketChannel(SelectorProvider* sp);
    
    ~UNSocketChannel( );

    UNSocketChannel& operator=(const UNSocketChannel& rhs);

    int open();
    int open(UNSocketAddress& remote);
    int open(int fd, UNSocketAddress& remote);
    int open(int fd);
    
    int valid_ops();

    int read(ByteBuffer& dst);
    int read(char* buf, int buf_len);

    int write(ByteBuffer& src);
    int write(char* buf, int buf_len);

    bool ensure_read_open();
    bool ensure_write_open();

    bool is_bound() ;
    UNSocketAddress address();


    int bind(const char* un_sock_path) ;

    bool is_connected();
    bool is_connection_pending();
    void ensure_open_and_unconnected();
    bool connect(UNSocketAddress& sa);
    bool connect(const char* un_sock_path);
    bool finish_connect();

    static  int check_connect(int fd, bool block, bool ready);
    static  int shutdown(int fd, int how);

    int shutdown_input();
    int shutdown_output();
    bool is_input_open();
    bool is_onput_open();

    void close();
    int    get_fd();
    void kill();

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
UNSocketChannel& UNSocketChannel::operator=(const UNSocketChannel& rhs)
{
    *(SelectableChannel*)this = *(SelectableChannel*)&rhs;
    _fd = dup(rhs._fd);
    _state = rhs._state;

    // Binding
     _address = rhs._address;

     _is_input_open  = rhs._is_input_open;
     _is_output_open = rhs._is_output_open;
     _ready_to_connect = rhs._ready_to_connect;

    return *this;
}
    
inline
int UNSocketChannel::get_fd()
{
    return _fd;
}

// socket options
// SO_KEEPALIVE
inline
bool  UNSocketChannel::keep_alive()
{
    return Net::get_bool_option(_fd, SOL_SOCKET,  SO_KEEPALIVE);
}

inline
void  UNSocketChannel::keep_alive(bool b)
{
    Net::set_bool_option(_fd, SOL_SOCKET,  SO_KEEPALIVE, b);
}

// SO_LINGER
inline
int  UNSocketChannel::linger()
{
    return Net::get_int_option(_fd, SOL_SOCKET,  SO_LINGER);
}

inline
void  UNSocketChannel::linger(int n)
{
    Net::set_int_option(_fd, SOL_SOCKET,  SO_LINGER, n);
}

// SO_OOBINLINE
inline
bool  UNSocketChannel::out_of_band_inline()
{
    return Net::get_bool_option(_fd, SOL_SOCKET,  SO_OOBINLINE);
}

inline
void  UNSocketChannel::out_of_band_inline(bool b)
{
    Net::set_bool_option(_fd, SOL_SOCKET,  SO_OOBINLINE, b);
}

// SO_RCVBUF
inline
int  UNSocketChannel::recv_buf_size()
{
    return Net::get_int_option(_fd, SOL_SOCKET,  SO_RCVBUF);
}

inline
void  UNSocketChannel::recv_buf_size(int n)
{
    Net::set_int_option(_fd, SOL_SOCKET,  SO_RCVBUF, n);
}

// SO_SNDBUF
inline
int  UNSocketChannel::send_buf_size()
{
    return Net::get_int_option(_fd, SOL_SOCKET,  SO_SNDBUF);
}

inline
void  UNSocketChannel::send_buf_size(int n)
{
    Net::set_int_option(_fd, SOL_SOCKET,  SO_SNDBUF, n);
}

// SO_REUSEADDR
inline
bool  UNSocketChannel::reuse_addr()
{
    return Net::get_bool_option(_fd, SOL_SOCKET,  SO_REUSEADDR);
}

inline
void  UNSocketChannel::reuse_addr(bool b)
{
    Net::set_bool_option(_fd, SOL_SOCKET,  SO_REUSEADDR, b);
}



#endif

