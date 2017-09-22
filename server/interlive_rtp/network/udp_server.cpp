#include "udp_server.h"

#include "../../util/util.h"
#include "../../util/log.h"
#include "../../util/access.h"
#include "../../util/levent.h"
#include "utils/memory.h"
#include "../target.h"
#include <assert.h>
#include <errno.h>

UDPSocketEvent::UDPSocketEvent() :
    func_EmptyQueue(NULL), func_FullQueue_FirstPacketSent(NULL)
{
}

UDPServerConfig::UDPServerConfig()
{
    manage_buffer = true;
    pop_bad_packet = true;
    recv_upon = true;
    auto_enable_write = true;
    max_packet_size = UPDSERV_PACKET_BUF_SIZE;
    send_queue_size = 2000;
    recv_queue_size = 1000;
    sock_snd_buf_size = 0;
    sock_rcv_buf_size = 0;
}

UDPServerConfig::UDPServerConfig(bool _manage_buffer, bool _pop_bad_packet, bool _recv_upon, bool _auto_enable_write,
    size_t _max_packet_size, size_t _send_queue_size, size_t _recv_queue_size,
    size_t _sock_snd_buf_size, size_t _sock_rcv_buf_size) :
    manage_buffer(_manage_buffer), pop_bad_packet(_pop_bad_packet), recv_upon(_recv_upon), auto_enable_write(_auto_enable_write), 
    max_packet_size(_max_packet_size), send_queue_size(_send_queue_size), recv_queue_size(_recv_queue_size), 
    sock_snd_buf_size(_sock_snd_buf_size), sock_rcv_buf_size(_sock_rcv_buf_size)
{
}

UDPPacket::UDPPacket() : ip(0), port(0), buf(NULL), size(0), arg(NULL), func_send_finished(NULL)
{
}

bool UDPPacket::operator<(const UDPPacket& rhv) const
{
    return (ip < rhv.ip) || (ip == rhv.ip && port < rhv.port);
}

UDPServer::UDPServer(const UDPServerConfig* config) :
    _config(), _listen_port(0), _sock_fd(0), 
    _sock_addr(), _ev_read(), _ev_write(), _main_base(NULL), _send_queue(), 
    us_event()
{
    bzero(&_sock_addr, sizeof(_sock_addr));
    if (config != NULL)
        memcpy(const_cast<UDPServerConfig*>(&_config), config, sizeof(UDPServerConfig));
}

UDPServer::~UDPServer()
{
}

bool UDPServer::start(int listen_port, struct event_base * main_base)
{
    if (_sock_fd != 0)
    {
        ERR("_sock_fd has already opened");
        return false;
    }

    _listen_port = listen_port;
    _main_base = main_base;

    _sock_addr.sin_family = AF_INET;
    _sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    _sock_addr.sin_port = htons(_listen_port);

    _sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    fcntl(_sock_fd, F_SETFD, FD_CLOEXEC);
    int ret = set_nonblock(_sock_fd, true);
    if (0 != ret) {
        ERR("set nonblock failed. ret = %d, fd = %d, error = %s", ret, _sock_fd, strerror(errno));
        //#todo close fd
        return false;
    }

    if (_config.sock_snd_buf_size != 0)
    {
        int sock_snd_buf_sz = _config.sock_snd_buf_size; // 512 * 1024;
        if (-1 == setsockopt(_sock_fd, SOL_SOCKET, SO_SNDBUF, (void*)&sock_snd_buf_sz, sizeof(sock_snd_buf_sz)))
        {
            ERR("set sock snd buf sz failed. error=%d %s", errno, strerror(errno));
            //#todo close fd
            return false;
        }
        //_config.sock_snd_buf_size = sock_snd_buf_sz;
    }

    if (_config.sock_rcv_buf_size != 0)
    {
        int sock_rcv_buf_sz = _config.sock_rcv_buf_size; // 4 * 1024 * 1024;
        if (-1 == setsockopt(_sock_fd, SOL_SOCKET, SO_RCVBUF, (void*)&sock_rcv_buf_sz, sizeof(sock_rcv_buf_sz)))
        {
            ERR("set sock rcv buf sz failed. error=%d %s", errno, strerror(errno));
            //#todo close fd
            return false;
        }
        //_config.sock_rcv_buf_size = sock_rcv_buf_sz;
    }

    ret = bind(_sock_fd, (struct sockaddr*)&_sock_addr, sizeof(_sock_addr));
    if (0 != ret) {
        ERR("bind socket error. ret = %d, fd = %d, error = %s",
            ret, _sock_fd, strerror(errno));
        return false;
    }

    event_set(&_ev_write, _sock_fd, EV_WRITE, _on_write, (void *)this);
    event_base_set(_main_base, &_ev_write);

    event_set(&_ev_read, _sock_fd, EV_READ | EV_PERSIST, _on_read, (void *)this);
    event_base_set(_main_base, &_ev_read);
    event_add(&_ev_read, 0);

    return true;
}

void UDPServer::stop()
{
    if (_sock_fd == 0)
    {
        ERR("_sock_fd not open");
        return;
    }

    while (! _send_queue.empty())
    {
        const UDPPacket& packet(_send_queue.UPQ_FIRST_ELEM_OP());

        udp_packet_send_finished_func sf_func = packet.func_send_finished;
        if (sf_func != NULL)
            sf_func(packet, UPFS_NotSend_Abandon);

        if (_config.manage_buffer)
        {
            // this clash with const declaration
            // due to priority_queue should keep its element returned by top() not modified
            UDPPacket& _packet(const_cast<UDPPacket&>(packet));
            delete[] _packet.buf;
            _packet.buf = NULL;
        }

        _send_queue.pop();
    }

    UDPSocketEvent _us_event;
    us_event = _us_event;

    event_del(&_ev_read);
    event_del(&_ev_write);
    close(_sock_fd);
    _sock_fd = 0;
}

bool UDPServer::send_packet(const UDPPacket& packet)
{
    if (packet.buf == NULL || packet.size == 0 || packet.ip == 0 || packet.port == 0)
        return false;

    if (_config.send_queue_size == 0)
        return send_packet_to_sys(packet);
    else
        return send_packet_to_queue(packet);
}

bool UDPServer::send_packet_to_sys(const UDPPacket& packet)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(packet.ip);
    addr.sin_port = htons(packet.port);

    int ret = sendto(_sock_fd, packet.buf, packet.size, 0, (struct sockaddr*)&addr, sizeof(addr));
    if (ret > 0)
    {
        if (packet.func_send_finished != NULL)
            packet.func_send_finished(packet, UPFS_SendOK);

        return true;
    }
    else
    {
        //if (errno == EAGAIN || errno == EWOULDBLOCK)
        //{
        //    WRN("UDPServer::send_queued_packet, sendto warning ret=%d, errno=%d, packet.size=%u, ip=%d, port=%d",
        //        ret, int(errno), uint32_t(packet.size), int(packet.ip), int(packet.port));
        //}
        //else
        //{
        //    ERR("UDPServer::send_queued_packet, sendto error ret=%d, errno=%d, packet.size=%u, ip=%d, port=%d",
        //        ret, int(errno), uint32_t(packet.size), int(packet.ip), int(packet.port));
        //}

        if (packet.func_send_finished != NULL)
            packet.func_send_finished(packet, UPFS_SendERR);

        return false;
    }
}

bool UDPServer::send_packet_to_queue(const UDPPacket& packet)
{
    if (queue_size() >= _config.send_queue_size)
    {
        if (_config.auto_enable_write)
            enable_write();
        return false;
    }

    if (_config.manage_buffer)
    {
        UDPPacket _packet(packet);
        char* old_buf = _packet.buf;
        _packet.buf = new char[_packet.size];
        memcpy(_packet.buf, old_buf, _packet.size);

        _send_queue.push(_packet);
    }
    else
    {
        _send_queue.push(packet);
    }

    if (_config.auto_enable_write)
        enable_write();
    return true;
}

void UDPServer::recv_packet(const UDPPacket& packet)
{
}

void UDPServer::on_write()
{
    while (send_queued_packet());

    if (us_event.func_OnWriteFinished != NULL)
        us_event.func_OnWriteFinished(USE_OnWriteFinished);
}

void UDPServer::disable_write()
{
    event_del(&_ev_write);
}

void UDPServer::enable_write()
{
    event_add(&_ev_write, NULL);
}

bool UDPServer::send_queued_packet()
{
    if (_send_queue.empty())
        return false;

    const UDPPacket& packet(_send_queue.UPQ_FIRST_ELEM_OP());
    const size_t old_que_size = _send_queue.size();

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(packet.ip);
    addr.sin_port = htons(packet.port);

    int ret = sendto(_sock_fd, packet.buf, packet.size, 0, (struct sockaddr*)&addr, sizeof(addr));
    if (ret > 0)
    {
        if (packet.func_send_finished != NULL)
            packet.func_send_finished(packet, UPFS_SendOK);

        if (_config.manage_buffer)
        {
            UDPPacket& _packet(const_cast<UDPPacket&>(packet));
            delete[] _packet.buf;
            _packet.buf = NULL;
        }

        // do not allow send_packet in above callback
        assert(old_que_size == _send_queue.size());
        _send_queue.pop();

        if (us_event.func_EmptyQueue != NULL && _send_queue.empty())
            us_event.func_EmptyQueue(USE_EmptyQueue);
        if (us_event.func_FullQueue_FirstPacketSent != NULL &&_send_queue.size() + 1 == _config.send_queue_size)
            us_event.func_FullQueue_FirstPacketSent(USE_FullQueue_FirstPacketSent);

        return true;
    }
    else
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            WRN("UDPServer::send_queued_packet, sendto warning ret=%d, errno=%d, packet.size=%u, ip=%d, port=%d", 
                ret, int(errno), uint32_t(packet.size), int(packet.ip), int(packet.port));
            enable_write();
        }
        else
        {
            ERR("UDPServer::send_queued_packet, sendto error ret=%d, errno=%d, packet.size=%u, ip=%d, port=%d", 
                ret, int(errno), uint32_t(packet.size), int(packet.ip), int(packet.port));

            if (packet.func_send_finished != NULL)
                packet.func_send_finished(packet, UPFS_SendERR);

            if (_config.pop_bad_packet)
            {
                if (_config.manage_buffer)
                {
                    UDPPacket& _packet(const_cast<UDPPacket&>(packet));
                    delete[] _packet.buf;
                    _packet.buf = NULL;
                }

                assert(old_que_size == _send_queue.size());
                _send_queue.pop();
            }
        }

        return false;
    }
}

int UDPServer::set_nonblock(int fd, bool is_set)
{
    int flags = 1;

    //    struct linger ling = { 0, 0 };

    if (fd > 0) {
        /*
        setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (void *) &flags,
        sizeof (flags));
        setsockopt (fd, SOL_SOCKET, SO_KEEPALIVE, (void *) &flags,
        sizeof (flags));
        setsockopt (fd, SOL_SOCKET, SO_LINGER, (void *) &ling, sizeof (ling));
        */
        flags = fcntl(fd, F_GETFL);
        if (-1 == flags)
            return -1;
        if (is_set)
            flags |= O_NONBLOCK;
        else
            flags &= ~O_NONBLOCK;
        if (0 != fcntl(fd, F_SETFL, flags))
            return -2;
    }
    return 0;
}

void UDPServer::on_read_queueOne()
{
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    socklen_t addr_len = sizeof(addr);
    char buf[UPDSERV_PACKET_BUF_SIZE];

    do 
    {
        ssize_t ret = recvfrom(_sock_fd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addr_len);
        if (ret < 0)
        {
            //ERR("UDPServer::recv_packet recvfrom error, ret=%d, errno=%d", int(ret), int(errno));
            return;
        }

        UDPPacket packet;
        packet.ip = ntohl(addr.sin_addr.s_addr);
        packet.port = ntohs(addr.sin_port);
        packet.buf = buf;
        packet.size = ret;
        packet.arg = this;

        recv_packet(packet);
    } while (_config.recv_upon);
}

void UDPServer::on_read_queueN()
{
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    socklen_t addr_len = sizeof(addr);
    char buf[UPDSERV_PACKET_BUF_SIZE];
    udp_packet_que_t _recv_queue;
    
    for (size_t cnt = _config.recv_queue_size; cnt > 0; cnt--)
    {
        ssize_t ret = recvfrom(_sock_fd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addr_len);
        if (ret < 0)
        {
            //ERR("UDPServer::recv_packet recvfrom error, ret=%d, errno=%d", int(ret), int(errno));
            break;
        }

        UDPPacket packet;
        packet.ip = ntohl(addr.sin_addr.s_addr);
        packet.port = ntohs(addr.sin_port);
        packet.buf = new char[ret];
        packet.size = ret;
        packet.arg = this;

        memmove(packet.buf, buf, ret);

        _recv_queue.push(packet);
    }

    while (!_recv_queue.empty())
    {
        const UDPPacket& packet(_recv_queue.UPQ_FIRST_ELEM_OP());

        recv_packet(packet);
        delete[] packet.buf;

        _recv_queue.pop();
    }
}

void UDPServer::_on_read(const int fd, const short which, void *arg)
{
    ASSERTV(arg != NULL);

    UDPServer * udp_server = (UDPServer *)arg;
    if ((udp_server->_config.recv_queue_size == 1) || !(udp_server->_config.recv_upon))
        udp_server->on_read_queueOne();
    else
        udp_server->on_read_queueN();
}

void UDPServer::_on_write(const int fd, const short which, void *arg)
{
    ASSERTV(arg != NULL);

    UDPServer * udp_server = (UDPServer *)arg;
    udp_server->on_write();
}

void UDPServer::invoke_enable_write()
{
    if (!_send_queue.empty())
        enable_write();
}

size_t UDPServer::queue_size() const
{
    return _send_queue.size();
}

UDPSocketEvent& UDPServer::get_us_event()
{
    return us_event;
}

const UDPServerConfig& UDPServer::get_config() const
{
    return _config;
}
