#include "stream_manager.h"

#include <netinet/in.h>
#include <appframe/singleton.hpp>
#include <util/log.h>
#include <util/util.h>

#include "common/protobuf/st2t_fpinfo_packet.pb.h"

#define FPINFO_BUFFER_SIZE 1024
using namespace std;
using namespace lcdn;
using namespace lcdn::net;
using namespace lcdn::base;

StreamManager::StreamManager() : _is_stream_manager_ready(false)
{
}

StreamManager::~StreamManager()
{
}

void StreamManager::insert(const StreamItem& item)
{
    _stream_map[item.stream_id] = item;
}

void StreamManager::update_if_existed(uint32_t stream_id, uint32_t last_ts, uint64_t block_seq)
{
    if (_stream_map.find(stream_id) != _stream_map.end())
    {
        _stream_map[stream_id].last_ts = last_ts;
        _stream_map[stream_id].block_seq = block_seq;
    }
}

void StreamManager::erase(uint32_t stream_id)
{
    _stream_map.erase(stream_id);
}

StreamMap& StreamManager::stream_map()
{
    return _stream_map;
}

void StreamManager::make_stream_manager_ready()
{
    _is_stream_manager_ready = true;
}

bool StreamManager::is_stream_manager_ready()
{
    return _is_stream_manager_ready;
}

StreamListNotifier::StreamListNotifier(EventLoop* loop, InetAddress& listen_addr,
                                       string name, StreamManager* stream_manager, ForwardServerManager* forward_manager)
    : _tcp_server(loop, listen_addr, name, this),
      _stream_manager(stream_manager),
      _keepalived_watcher(this),
      _forward_manager(forward_manager)
{
}

StreamListNotifier::~StreamListNotifier()
{}

void StreamListNotifier::start()
{
    EventLoop* loop = _tcp_server.event_loop();
    loop->run_forever(keepalived_timer_interval, &_keepalived_watcher);

    _tcp_server.start();
}

void StreamListNotifier::notify_stream_list(TCPConnection* connection)
{
    Buffer obuf(10 * 1024 * 1024);

    uint16_t stream_cnt = _stream_manager->stream_map().size();
    uint32_t total_sz = sizeof(proto_header) + sizeof(p2f_inf_stream_v2)
        + stream_cnt * sizeof(stream_info);

    // encode header
    uint16_t ncmd = htons(CMD_P2F_INF_STREAM_V2);
    uint32_t nsize = htonl(total_sz);

    obuf.append_ptr(&MAGIC_1, sizeof(uint8_t));
    obuf.append_ptr(&VER_1, sizeof(uint8_t));
    obuf.append_ptr(&ncmd, sizeof(uint16_t));
    obuf.append_ptr(&nsize, sizeof(uint32_t));

    // encode payload
    uint16_t nstream_cnt = htons(stream_cnt);
    obuf.append_ptr(&nstream_cnt, sizeof(uint16_t));

    StreamMap stream_map = _stream_manager->stream_map();
    StreamMap::iterator it;

    for (it = stream_map.begin(); it != stream_map.end(); it++)
    {
        uint32_t nstream_id = htonl((it->second).stream_id);
        __time_t ntv_sec = util_htonll((it->second).start_time.tv_sec);
        __suseconds_t ntv_usec = util_htonll((it->second).start_time.tv_usec);
        obuf.append_ptr(&nstream_id, sizeof(uint32_t));
        obuf.append_ptr(&ntv_sec, sizeof(__time_t));
        obuf.append_ptr(&ntv_usec, sizeof(__suseconds_t));
    }

    connection->send(&obuf);
}

void StreamListNotifier::notify_stream_start(StreamItem& item)
{
    Buffer obuf(1024);

    uint32_t total_sz = sizeof(proto_header) + sizeof(p2f_start_stream_v2);

    // encode header
    uint16_t ncmd = htons(CMD_P2F_START_STREAM_V2);
    uint32_t nsize = htonl(total_sz);

    obuf.append_ptr(&MAGIC_1, sizeof(uint8_t));
    obuf.append_ptr(&VER_1, sizeof(uint8_t));
    obuf.append_ptr(&ncmd, sizeof(uint16_t));
    obuf.append_ptr(&nsize, sizeof(uint32_t));

    // encode payload
    p2f_start_stream_v2 pss;
    pss.streamid = htonl(item.stream_id);
    pss.start_time.tv_sec = util_htonll(item.start_time.tv_sec);
    pss.start_time.tv_usec = util_htonll(item.start_time.tv_usec);

    obuf.append_ptr(&pss, sizeof(p2f_start_stream_v2));

    send_to_all(&obuf);
}

void StreamListNotifier::notify_stream_close(uint32_t stream_id)
{
    Buffer obuf(1024);

    uint32_t total_sz = sizeof(proto_header) + sizeof(p2f_close_stream);

    // encode header
    uint16_t ncmd = htons(CMD_P2F_CLOSE_STREAM);
    uint32_t nsize = htonl(total_sz);

    obuf.append_ptr(&MAGIC_1, sizeof(uint8_t));
    obuf.append_ptr(&VER_1, sizeof(uint8_t));
    obuf.append_ptr(&ncmd, sizeof(uint16_t));
    obuf.append_ptr(&nsize, sizeof(uint32_t));

    // encode payload
    p2f_close_stream pcs;
    pcs.streamid = htonl(stream_id);

    obuf.append_ptr(&pcs, sizeof(p2f_close_stream));

    send_to_all(&obuf);
}

void StreamListNotifier::send_to_all(Buffer* buffer)
{
    map<uint32_t, TCPConnection*>::iterator it;

    for (it = _conn_map.begin(); it != _conn_map.end(); it++)
    {
        TCPConnection* connection = it->second;
        connection->send(buffer);
    }
}

void StreamListNotifier::send(Buffer* buffer, TCPConnection *connection)
{
    if (NULL != connection)
    {
        connection->send(buffer);
    }
}


bool StreamListNotifier::is_frame(Buffer* inbuf, proto_header& header)
{
    assert(NULL != inbuf);

    if (inbuf->data_len() < sizeof(proto_header))
    {
        return false;
    }

    proto_header *ih = (proto_header*)(inbuf->data_ptr());

    header.magic = ih->magic;
    header.version = ih->version;
    header.cmd = ntohs(ih->cmd);
    header.size = ntohl(ih->size);

    if (header.size > inbuf->data_len())
    {
        return false;
    }

    return true;
}

void StreamListNotifier::handle_frame(Buffer* inbuf, proto_header& header, TCPConnection* connection)
{
    switch (header.cmd)
    {
    case CMD_R2P_KEEPALIVE:
    {
        size_t offset = 0;
        r2p_keepalive r2p;
        r2p_keepalive* r2p_tmp = (r2p_keepalive *)(buffer_data_ptr(inbuf) + offset);
        r2p.listen_uploader_addr = r2p_tmp->listen_uploader_addr;
        r2p.listen_uploader_addr.ip = ntohl(r2p_tmp->listen_uploader_addr.ip);
        r2p.listen_uploader_addr.port = ntohs(r2p_tmp->listen_uploader_addr.port);
        r2p.outbound_speed = ntohl(r2p_tmp->outbound_speed);
        r2p.inbound_speed = ntohl(r2p_tmp->inbound_speed);
        r2p.stream_cnt = ntohl(r2p_tmp->stream_cnt);
        offset += sizeof(r2p_keepalive);

        for (int i = 0; i < r2p.stream_cnt; i++)
        {
            receiver_stream_status* rss_tmp = (receiver_stream_status*)(buffer_data_ptr(inbuf) + offset);

            uint32_t stream_id = ntohl(rss_tmp->streamid);
            _stream_manager->update_if_existed(stream_id, ntohl(rss_tmp->last_ts), util_ntohll(rss_tmp->block_seq));

            offset += sizeof(receiver_stream_status);
        }
        break;
    }
    case CMD_F2P_WHITE_LIST_REQ:
    {
        if (_stream_manager->is_stream_manager_ready())
            notify_stream_list(connection);
        break;
    }
    case CMD_ST2T_FPINFO_REQ:
    {
        st2t_fpinfo_packet fpinfos;
        fpinfos = _forward_manager->fm_get_forward_fp_infos();
        size_t size = 0;
        char buffer[FPINFO_BUFFER_SIZE] = "";

        size = fpinfos.ByteSize();

        fpinfos.SerializeToArray(buffer, size);

        size_t total_size = size + sizeof(proto_header);

        if (encode_header_v3(inbuf, CMD_T2ST_FPINFO_RSP, total_size) != 0)
        {
            ERR("Portal::parse_cmd:encode header error.");
            return;
        }

        int status = buffer_append_ptr(inbuf, (void*)buffer, size);
        if (status == -1)
        {
            ERR("Portal::parse_cmd:encode error.");
            return;
        }

        send(inbuf, connection);
        break;
    }
    default:
        break;
    }
    return;
}

// virtual
void StreamListNotifier::on_connect(TCPConnection* connection)
{
    time_t* last_time = new time_t;
    *last_time = time(NULL);

    connection->set_context(last_time);
    connection->set_input_buffer_size(input_buffer_size);
    connection->set_output_buffer_size(output_buffer_size);
    _conn_map[connection->id()] = connection;

}

// virtual
void StreamListNotifier::on_read(TCPConnection* connection)
{
    assert(NULL != connection);

    bool is_frame_flag = false;
    proto_header header;
    uint32_t header_size = sizeof(proto_header);

    lcdn::base::Buffer* buf = connection->input_buffer();
    is_frame_flag = is_frame(buf, header);

    while (is_frame_flag)
    {
        // update last reading cmd frame time
        time_t* last_time = static_cast<time_t*>(connection->context());
        *last_time = time(NULL);

        // hand over to fsm to handle read
        buf->eat(header_size);

        handle_frame(buf, header, connection);

        buf->eat(header.size - header_size);

        is_frame_flag = is_frame(buf, header);
    }

    buf->adjust();
}

// virtual
void StreamListNotifier::on_write_completed(TCPConnection* connection)
{
}

// virtual
void StreamListNotifier::on_close(TCPConnection* connection)
{
    time_t* last_time = static_cast<time_t*>(connection->context());
    *last_time = time(NULL);

    if (last_time)
    {
        delete last_time;
        last_time = NULL;
    }
    _conn_map.erase(connection->id());
}

void StreamListNotifier::_check_connection()
{
    ConnectionMap::iterator it;

    for (it = _conn_map.begin(); it != _conn_map.end(); )
    {
        TCPConnection* connection = it->second;
        it++;
        time_t* last_time = static_cast<time_t*>(connection->context());
        if (time(NULL) - *last_time > connection_timeout)
        {
            _tcp_server.close(connection->id());
        }
    }
}
