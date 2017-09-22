#ifndef _UDP_SERVER_H_
#define _UDP_SERVER_H_

#include <string>
#include <queue>
//#include <ext/hash_map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include "utils/buffer.hpp"
#include "../../util/levent.h"

#define UPDSERV_PACKET_BUF_SIZE 1600
//#define UPDSERV_CLUSTER_ENDP

enum udp_socket_event
{
    USE_EmptyQueue = 0,
    USE_FullQueue_FirstPacketSent,
    USE_OnWriteFinished
};

enum udp_packet_finish_state
{
    UPFS_SendOK = 0,
    UPFS_SendERR,
    UPFS_NotSend_Abandon
};

struct UDPPacket;
typedef void(*udp_packet_send_finished_func)(const UDPPacket& packet, udp_packet_finish_state state);
typedef void(*udp_socket_event_func)(udp_socket_event e);

struct UDPSocketEvent
{
    udp_socket_event_func func_EmptyQueue;
    udp_socket_event_func func_FullQueue_FirstPacketSent;
    udp_socket_event_func func_OnWriteFinished;

    UDPSocketEvent();
};

struct UDPServerConfig
{
    bool manage_buffer;
    bool pop_bad_packet;
    bool recv_upon;
    bool auto_enable_write;
    size_t max_packet_size;
    size_t send_queue_size;
    size_t recv_queue_size;
    size_t sock_snd_buf_size;
    size_t sock_rcv_buf_size;

    UDPServerConfig();
    UDPServerConfig(bool _manage_buffer, bool _pop_bad_packet, bool _recv_upon, bool _auto_enable_write,
        size_t _max_packet_size, size_t _send_queue_size, size_t _recv_queue_size, 
        size_t _sock_snd_buf_size, size_t _sock_rcv_buf_size);
};

struct UDPPacket
{
    in_addr_t ip;
    short port;
    char* buf;
    size_t size;
    void* arg;
    udp_packet_send_finished_func func_send_finished;

    UDPPacket();
    bool operator<(const UDPPacket& rhv) const;
};

#ifdef UPDSERV_CLUSTER_ENDP
typedef std::priority_queue<UDPPacket> udp_packet_que_t;
#define UPQ_FIRST_ELEM_OP top
#else
typedef std::queue<UDPPacket> udp_packet_que_t;
#define UPQ_FIRST_ELEM_OP front
#endif

class UDPServer
{
public:
    UDPServer(const UDPServerConfig* config = NULL);
    virtual ~UDPServer();
    bool start(int listen_port, struct event_base * main_base);
    void stop();

    bool send_packet(const UDPPacket& packet);
    virtual void recv_packet(const UDPPacket& packet);

    void invoke_enable_write();

    size_t queue_size() const;

    UDPSocketEvent& get_us_event();
    const UDPServerConfig& get_config() const;

private:
    static void _on_read(const int fd, const short which, void *arg);
    static void _on_write(const int fd, const short which, void *arg);

    void on_read_queueOne();
    void on_read_queueN();
    void on_write();

    void disable_write();
    void enable_write();

    bool send_packet_to_sys(const UDPPacket& packet);
    bool send_packet_to_queue(const UDPPacket& packet);

    bool send_queued_packet();
    int set_nonblock(int fd, bool is_set);

    const UDPServerConfig _config;
    int _listen_port;
    int _sock_fd;

    struct sockaddr_in _sock_addr;
    struct event _ev_read, _ev_write;
    struct event_base * _main_base;

    udp_packet_que_t _send_queue;

    UDPSocketEvent us_event;
};

#endif
