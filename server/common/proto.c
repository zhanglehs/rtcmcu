#define _BSD_SOURCE
#include "proto.h"
#include <endian.h>
#include <stdint.h>
#include <assert.h>
#include <arpa/inet.h>
#include "utils/buffer.hpp"
#include "util/util.h"
#include "util/log.h"

#ifdef _WIN32
#define assert(x) (x)
#endif

#define MAKE_BUF_USABLE(obuf, total_sz) \
{\
    int ret = 0; \
if (buffer_capacity(obuf) < total_sz)\
{\
    ret = buffer_expand_capacity(obuf, total_sz); \
if (0 != ret)\
    return ret; \
}\
}

static int encode_make_buf_usable(buffer* buf, uint32_t total_sz)
{
    MAKE_BUF_USABLE(buf, total_sz);

    return total_sz;
}

void
encode_header(buffer * obuf, uint16_t cmd, uint32_t size)
{
    uint16_t ncmd = htons(cmd);
    uint32_t nsize = htonl(size);

    buffer_append_ptr(obuf, &MAGIC_1, sizeof(uint8_t));
    buffer_append_ptr(obuf, &VER_1, sizeof(uint8_t));
    buffer_append_ptr(obuf, &ncmd, sizeof(uint16_t));
    buffer_append_ptr(obuf, &nsize, sizeof(uint32_t));
}

void encode_header_rtp(char* obuf, size_t obuf_size, const proto_header& header)
{
    ASSERTV(obuf != NULL && obuf_size >= sizeof(proto_header));

    proto_header* ph_obuf = (proto_header*)obuf;
    ph_obuf->magic = header.magic;
    ph_obuf->version = header.version;
    ph_obuf->cmd = htons(header.cmd);
    ph_obuf->size = htonl(header.size);
}

void encode_header_uint8(uint8_t* obuf, uint32_t& len, uint16_t cmd, uint32_t size)
{
    uint16_t ncmd = htons(cmd);
    uint32_t nsize = htonl(size);

    obuf[0] = MAGIC_1;
    obuf[1] = VER_1;
    memcpy(&obuf[2], &ncmd, 2);
    memcpy(&obuf[4], &nsize, 4);

    len = sizeof(proto_header);
}

int decode_header_uint8(const uint8_t * ibuf, uint32_t len, proto_header * h)
{
    if (len < sizeof(proto_header))
        return -1;
    proto_header *ih = (proto_header *)ibuf;

    switch (ih->version)
    {
    case VER_1:
        break;

    case VER_2:
        if (ih->magic != MAGIC_2)
            return -1;
        break;

    case VER_3:
        if (ih->magic != MAGIC_3)
            return -1;
        break;

    default:
        return -1;
    }

    if (h != NULL)
    {
        h->magic = ih->magic;
        h->version = ih->version;
        h->cmd = ntohs(ih->cmd);
        h->size = ntohl(ih->size);
    }
    else
    {
        return -1;
    }

    return 0;
}

int decode_header(const buffer * ibuf, proto_header * h)
{
    if (buffer_data_len(ibuf) < sizeof(proto_header))
        return -1;
    proto_header *ih = (proto_header *)buffer_data_ptr(ibuf);

    switch (ih->version)
    {
    case VER_1:
        break;

    case VER_2:
        if (ih->magic != MAGIC_2)
            return -1;
        break;

    case VER_3:
        if (ih->magic != MAGIC_3)
            return -1;
        break;

    default:
        return -1;
    }

    if (h != NULL)
    {
        h->magic = ih->magic;
        h->version = ih->version;
        h->cmd = ntohs(ih->cmd);
        h->size = ntohl(ih->size);
    }
    else
    {
        return -1;
    }

	return 0;
}

int decode_header_rtp(const char * ibuf, size_t ibuf_size, proto_header& h)
{
    ASSERTR(ibuf != NULL && ibuf_size >= sizeof(proto_header), -1);

    proto_header* ph_ibuf = (proto_header*)ibuf;
    h.magic = ph_ibuf->magic;
    h.version = ph_ibuf->version;
    h.cmd = ntohs(ph_ibuf->cmd);
    h.size = ntohl(ph_ibuf->size);

    return 0;
}

int decode_f2t_header_v2(const buffer * ibuf, proto_header * header, mt2t_proto_header * tracker_header)
{
    int ret = decode_header(ibuf, header);
    if (ret != 0)
        return ret;

    if (header->size < sizeof(proto_header)+sizeof(mt2t_proto_header))
        return -1;

    mt2t_proto_header* ih = (mt2t_proto_header*)(buffer_data_ptr(ibuf) + sizeof(proto_header));

    if (tracker_header != NULL)
    {
        tracker_header->seq = ntohs(ih->seq);
        tracker_header->reserved = ntohs(ih->reserved);
    }

    return 0;
}

int decode_f2t_header_v3(const buffer * ibuf, proto_header * header, mt2t_proto_header * tracker_header)
{
    return decode_f2t_header_v2(ibuf, header, tracker_header);
}

int decode_f2t_header_v4(const buffer * ibuf, proto_header * header, mt2t_proto_header * tracker_header)
{
    return decode_f2t_header_v2(ibuf, header, tracker_header);
}

int encode_header_v2(buffer * obuf, proto_t cmd, uint32_t size)
{
    ASSERTR(obuf != NULL, -1);

    uint16_t ncmd = htons(cmd);
    uint32_t nsize = htonl(size);
    MAKE_BUF_USABLE(obuf, sizeof(proto_header));

    int ret = 0;
    ret += buffer_append_ptr(obuf, &MAGIC_2, sizeof(uint8_t));
    ret += buffer_append_ptr(obuf, &VER_2, sizeof(uint8_t));
    ret += buffer_append_ptr(obuf, &ncmd, sizeof(uint16_t));
    ret += buffer_append_ptr(obuf, &nsize, sizeof(uint32_t));

    return ret;
}

int encode_header_v3(buffer * obuf, proto_t cmd, uint32_t size)
{
    ASSERTR(obuf != NULL, -1);

    uint16_t ncmd = htons(cmd);
    uint32_t nsize = htonl(size);
    MAKE_BUF_USABLE(obuf, sizeof(proto_header));

    int ret = 0;
    ret += buffer_append_ptr(obuf, &MAGIC_3, sizeof(uint8_t));
    ret += buffer_append_ptr(obuf, &VER_3, sizeof(uint8_t));
    ret += buffer_append_ptr(obuf, &ncmd, sizeof(uint16_t));
    ret += buffer_append_ptr(obuf, &nsize, sizeof(uint32_t));

    return ret;
}

int encode_header_v2_s(buffer * obuf, proto_t cmd, uint32_t*& psize)
{
    ASSERTR(obuf != NULL, -1);

    uint16_t ncmd = htons(cmd);
    uint32_t nsize = htonl(0); // keep space
    MAKE_BUF_USABLE(obuf, sizeof(proto_header));

    int ret = 0;
    ret += buffer_append_ptr(obuf, &MAGIC_2, sizeof(uint8_t));
    ret += buffer_append_ptr(obuf, &VER_2, sizeof(uint8_t));
    ret += buffer_append_ptr(obuf, &ncmd, sizeof(uint16_t));
    ret += buffer_append_ptr(obuf, &nsize, sizeof(uint32_t));

    // point back to the last field
    psize = (uint32_t*)(obuf->_ptr + (obuf->_end - sizeof(uint32_t)));
    return ret;
}

int encode_header_v3_s(buffer * obuf, proto_t cmd, uint32_t*& psize)
{
    ASSERTR(obuf != NULL, -1);

    uint16_t ncmd = htons(cmd);
    uint32_t nsize = htonl(0); // keep space
    MAKE_BUF_USABLE(obuf, sizeof(proto_header));

    int ret = 0;
    ret += buffer_append_ptr(obuf, &MAGIC_3, sizeof(uint8_t));
    ret += buffer_append_ptr(obuf, &VER_3, sizeof(uint8_t));
    ret += buffer_append_ptr(obuf, &ncmd, sizeof(uint16_t));
    ret += buffer_append_ptr(obuf, &nsize, sizeof(uint32_t));

    // point back to the last field
    psize = (uint32_t*)(obuf->_ptr + (obuf->_end - sizeof(uint32_t)));
    return ret;
}

static inline int encode_const_field(char* value, size_t n, buffer * obuf)
{
    ASSERTR(obuf != NULL, -1);

    return buffer_append_ptr(obuf, value, n);
}

static inline int encode_const_field(uint8_t* value, size_t n, buffer * obuf)
{
    ASSERTR(obuf != NULL, -1);

    return buffer_append_ptr(obuf, value, n);
}

static inline int encode_const_field(uint64_t value, buffer * obuf)
{
    ASSERTR(obuf != NULL, -1);

    value = util_htonll(value);
    return buffer_append_ptr(obuf, &value, sizeof(value));
}

static inline int encode_const_field(uint32_t value, buffer * obuf)
{
    ASSERTR(obuf != NULL, -1);

    value = htonl(value);
    return buffer_append_ptr(obuf, &value, sizeof(value));
}

static inline int encode_const_field(uint16_t value, buffer * obuf)
{
    ASSERTR(obuf != NULL, -1);

    value = htons(value);
    return buffer_append_ptr(obuf, &value, sizeof(value));
}

static inline int encode_const_field(uint8_t value, buffer * obuf)
{
    ASSERTR(obuf != NULL, -1);

    return buffer_append_ptr(obuf, &value, sizeof(value));
}

int encode_mt2t_header(buffer * obuf, const mt2t_proto_header& tracker_header)
{
    ASSERTR(obuf != NULL, -1);

    MAKE_BUF_USABLE(obuf, sizeof(mt2t_proto_header));

    encode_const_field(tracker_header.seq, obuf);
    encode_const_field(tracker_header.reserved, obuf);

    return 0;
}

//forward <---> forward

int
encode_f2f_req_stream(const f2f_req_stream * body, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2f_req_stream);

    MAKE_BUF_USABLE(obuf, total_sz);
    uint32_t streamid = htonl(body->streamid);
    uint64_t seq = util_htonll(body->last_block_seq);

    encode_header(obuf, CMD_FC2FS_REQ_STREAM, total_sz);
    buffer_append_ptr(obuf, &streamid, sizeof(streamid));
    buffer_append_ptr(obuf, &seq, sizeof(seq));
    return 0;
}

int
decode_f2f_req_stream(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t total_sz = sizeof(proto_header)+sizeof(f2f_req_stream);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (h.size < total_sz || buffer_data_len(ibuf) < total_sz)
        return -2;
    f2f_req_stream *body = (f2f_req_stream *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->streamid = ntohl(body->streamid);
    body->last_block_seq = ntohs(body->last_block_seq);
    return 0;
}

int
encode_f2f_unreq_stream(const f2f_unreq_stream * body, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2f_unreq_stream);

    MAKE_BUF_USABLE(obuf, total_sz);
    uint32_t streamid = htonl(body->streamid);

    encode_header(obuf, CMD_FC2FS_UNREQ_STREAM, total_sz);
    buffer_append_ptr(obuf, &streamid, sizeof(streamid));
    return 0;
}

int
decode_f2f_unreq_stream(buffer * ibuf)
{
    size_t total_sz = sizeof(proto_header)+sizeof(f2f_unreq_stream);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (h.size < total_sz || buffer_data_len(ibuf) < total_sz)
        return -2;
    f2f_unreq_stream *body = (f2f_unreq_stream *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->streamid = ntohl(body->streamid);
    return 0;
}

int
encode_f2f_rsp_stream(const f2f_rsp_stream * body, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2f_rsp_stream);

    MAKE_BUF_USABLE(obuf, total_sz);
    encode_header(obuf, CMD_FS2FC_RSP_STREAM, total_sz);
    uint32_t streamid = htonl(body->streamid);
    uint16_t result = htons(body->result);

    buffer_append_ptr(obuf, &streamid, sizeof(streamid));
    buffer_append_ptr(obuf, &result, sizeof(result));
    return 0;
}

int
decode_f2f_rsp_stream(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t total_sz = sizeof(proto_header)+sizeof(f2f_rsp_stream);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (h.size < total_sz || buffer_data_len(ibuf) < total_sz)
        return -2;
    f2f_rsp_stream *body = (f2f_rsp_stream *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->streamid = ntohl(body->streamid);
    body->result = ntohs(body->result);
    return 0;
}

int
encode_f2f_streaming_header(const f2f_streaming_header * body,
buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2f_streaming_header)
        +body->payload_size;

    int ret = encode_make_buf_usable(obuf, total_sz);
    if (ret < 0)
    {
        return 0;
    }

    encode_header(obuf, CMD_FS2FC_STREAMING_HEADER, total_sz);
    uint32_t streamid = htonl(body->streamid);
    uint32_t size = htonl(body->payload_size);

    buffer_append_ptr(obuf, &streamid, sizeof(streamid));
    buffer_append_ptr(obuf, &size, sizeof(size));
    buffer_append_ptr(obuf, &body->payload_type, sizeof(uint8_t)
        +body->payload_size);
    int len = sizeof(streamid)+sizeof(size)+sizeof(uint8_t)
        +body->payload_size;
    return len;
}

int
decode_f2f_streaming_header(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t min_sz = sizeof(proto_header)+sizeof(f2f_streaming_header);

    if (buffer_data_len(ibuf) < min_sz)
        return -1;
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -2;
    f2f_streaming_header *body =
        (f2f_streaming_header *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->streamid = ntohl(body->streamid);
    body->payload_size = ntohl(body->payload_size);
    size_t actual_sz = min_sz + body->payload_size;

    if (buffer_data_len(ibuf) < actual_sz || h.size < actual_sz)
        return -3;
    return 0;
}

int
encode_f2f_start_task(const f2f_start_task * body, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2f_start_task)+body->payload_size;

    //MAKE_BUF_USABLE(obuf, total_sz);
    if (buffer_capacity(obuf) < total_sz)
    {
        return -1;
    }
    uint32_t task_id = htonl(body->task_id);
    uint32_t stream_id = htonl(body->stream_id);
    uint32_t payload_size = htonl(body->payload_size);

    TRC("encode_f2f_start_task: total_sz: %u, payload_size: %u",
        total_sz, body->payload_size);
    encode_header(obuf, CMD_FC2FS_START_TASK, total_sz);
    buffer_append_ptr(obuf, &task_id, sizeof(task_id));
    buffer_append_ptr(obuf, &stream_id, sizeof(stream_id));
    buffer_append_ptr(obuf, &payload_size, sizeof(payload_size));
    buffer_append_ptr(obuf, &body->payload, body->payload_size);

    return 0;
}

int
decode_f2f_start_task(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t total_sz = sizeof(proto_header)+sizeof(f2f_start_task);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (h.size < total_sz || buffer_data_len(ibuf) < total_sz)
        return -2;
    f2f_start_task *body = (f2f_start_task *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->task_id = ntohl(body->task_id);
    body->stream_id = ntohl(body->stream_id);
    body->payload_size = ntohl(body->payload_size);

    TRC("decode_f2f_start_task: total_sz: %lu, payload_size: %d",
        total_sz, body->payload_size);

    return 0;
}

int
encode_f2f_start_task_rsp(const f2f_start_task_rsp * body, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2f_start_task_rsp);

    //MAKE_BUF_USABLE(obuf, total_sz);
    if (buffer_capacity(obuf) < total_sz)
    {
        return -1;
    }
    uint32_t task_id = htonl(body->task_id);
    uint32_t result = htonl(body->result);

    TRC("encode_f2f_start_task_rsp: total_sz: %u, payload_size: %u",
        total_sz, 0);
    encode_header(obuf, CMD_FS2FC_START_TASK_RSP, total_sz);
    buffer_append_ptr(obuf, &task_id, sizeof(task_id));
    buffer_append_ptr(obuf, &result, sizeof(result));

    return 0;
}

int
decode_f2f_start_task_rsp(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t total_sz = sizeof(proto_header)+sizeof(f2f_start_task_rsp);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (h.size < total_sz || buffer_data_len(ibuf) < total_sz)
        return -2;
    f2f_start_task_rsp *body = (f2f_start_task_rsp *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->task_id = ntohl(body->task_id);
    body->result = ntohl(body->result);

    TRC("decode_f2f_start_task_rsp: total_sz: %lu, payload_size: %d",
        total_sz, 0);
    return 0;
}

int
encode_f2f_stop_task(const f2f_stop_task * body, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2f_stop_task);

    //MAKE_BUF_USABLE(obuf, total_sz);
    if (buffer_capacity(obuf) < total_sz)
    {
        //assert(0);
        return -1;
    }
    uint32_t task_id = htonl(body->task_id);
    uint32_t result = htonl(body->result);

    TRC("encode_f2f_stop_task: total_sz: %u, payload_size: %u",
        total_sz, 0);
    encode_header(obuf, CMD_F2F_STOP_TASK, total_sz);
    buffer_append_ptr(obuf, &task_id, sizeof(task_id));
    buffer_append_ptr(obuf, &result, sizeof(result));

    return 0;
}

int
decode_f2f_stop_task(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));

    size_t total_sz = sizeof(proto_header)+sizeof(f2f_stop_task);
    proto_header h;
    if (0 != decode_header(ibuf, &h))
        return -1;

    if (h.size < total_sz || buffer_data_len(ibuf) < total_sz)
        return -2;

    f2f_stop_task *body = (f2f_stop_task *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->task_id = ntohl(body->task_id);
    body->result = ntohl(body->result);

    TRC("decode_f2f_stop_task: total_sz: %lu, payload_size: %d",
        total_sz, 0);
    return 0;
}

/*
int
encode_f2f_stop_task_rsp(const f2f_stop_task_rsp * body, buffer * obuf)
{
uint32_t total_sz = sizeof(proto_header)+sizeof(f2f_stop_task_rsp);

MAKE_BUF_USABLE(obuf, total_sz);
uint32_t task_id = htonl(body->task_id);
uint8_t result = body->result;

encode_header(obuf, CMD_F2F_STOP_TASK_RSP, total_sz);
buffer_append_ptr(obuf, &task_id, sizeof(task_id));
buffer_append_ptr(obuf, &result, sizeof(result));

return 0;
}
*/

/*
int
decode_f2f_stop_task_rsp(buffer * ibuf)
{
// TODO
return -1;
//return decode_f2f_start_task_rsp(ibuf);
}
*/

int
encode_f2f_trans_data_piece(const f2f_trans_data * body, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2f_trans_data)
        +body->payload_size;

    //MAKE_BUF_USABLE(obuf, total_sz);
    if (buffer_capacity(obuf) < total_sz)
    {
        // TODO
        //assert(0);
        return -1;
    }
    uint32_t task_id = htonl(body->task_id);
    uint32_t payload_size = htonl(body->payload_size);
    //uint8_t task_status = body->task_status;

    TRC("encode_f2f_trans_data_piece: total_sz: %u, payload_size: %u",
        total_sz, body->payload_size);
    encode_header(obuf, CMD_FS2FC_STREAM_DATA, total_sz);
    buffer_append_ptr(obuf, &task_id, sizeof(task_id));
    //buffer_append_ptr(obuf, &task_status, sizeof(task_status) );
    buffer_append_ptr(obuf, &payload_size, sizeof(payload_size));
    buffer_append_ptr(obuf, &body->payload, body->payload_size);

    return 0;
}

int
decode_f2f_trans_data_piece(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t total_sz = sizeof(proto_header)+sizeof(f2f_trans_data);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (h.size < total_sz || buffer_data_len(ibuf) < total_sz)
        return -2;
    f2f_trans_data *body = (f2f_trans_data *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->task_id = ntohl(body->task_id);
    body->payload_size = ntohl(body->payload_size);

    TRC("decode_f2f_trans_data_piece: total_sz: %lu, payload_size: %d",
        total_sz, body->payload_size);

    return 0;
}

int
encode_f2f_streaming(const f2f_streaming * body, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2f_streaming)
        +body->payload_size;

    int ret = encode_make_buf_usable(obuf, total_sz);

    if (ret < 0)
    {
        return 0;
    }

    encode_header(obuf, CMD_FS2FC_STREAMING, total_sz);
    uint32_t streamid = htonl(body->streamid);
    uint64_t seq = util_htonll(body->seq);
    uint32_t size = htonl(body->payload_size);

    buffer_append_ptr(obuf, &streamid, sizeof(streamid));
    buffer_append_ptr(obuf, &seq, sizeof(seq));
    buffer_append_ptr(obuf, &size, sizeof(size));
    buffer_append_ptr(obuf, &body->payload_type, sizeof(uint8_t)
        +body->payload_size);
    int len = sizeof(streamid)+sizeof(seq)+sizeof(size)+sizeof(uint8_t)
        +body->payload_size;
    return len;
}

int
decode_f2f_streaming(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t min_sz = sizeof(proto_header)+sizeof(f2f_streaming);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (buffer_data_len(ibuf) < min_sz)
        return -2;
    f2f_streaming *body = (f2f_streaming *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->payload_size = ntohl(body->payload_size);
    size_t actual_sz = min_sz + body->payload_size;

    if (buffer_data_len(ibuf) < actual_sz || h.size < actual_sz)
        return -3;
    body->streamid = ntohl(body->streamid);
    body->seq = util_ntohll(body->seq);
    return 0;
}

//forward <---> forward v3 (support long stream id)

int
encode_f2f_start_task_v3(const f2f_start_task_v3 * body, buffer * obuf)
{
    ASSERTR(body != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    uint32_t total_sz = sizeof(proto_header) + sizeof(f2f_start_task_v3)
        + body->payload_size;

    MAKE_BUF_USABLE(obuf, total_sz);

    encode_header_v3(obuf, CMD_FC2FS_START_TASK_V3, total_sz);

    int ret = 0;
    ret += encode_const_field(body->task_id, obuf);
    ret += encode_const_field((uint8_t *)body->streamid, STREAM_ID_LEN, obuf);
    ret += encode_const_field(body->payload_size, obuf);
    ret += encode_const_field((uint8_t *)body->payload, body->payload_size, obuf);

    return ret;
}

int
decode_f2f_start_task_v3(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t total_sz = sizeof(proto_header)+sizeof(f2f_start_task_v3);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (h.size < total_sz || buffer_data_len(ibuf) < total_sz)
        return -2;
    f2f_start_task_v3 *body = (f2f_start_task_v3 *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->task_id = ntohl(body->task_id);
    //body->stream_id = ntohl(body->stream_id);
    body->payload_size = ntohl(body->payload_size);

    return 0;
}

int
encode_f2f_start_task_rsp_v3(const f2f_start_task_rsp_v3 * body, buffer * obuf)
{
    ASSERTR(body != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    uint32_t total_sz = sizeof(proto_header) + sizeof(f2f_start_task_rsp_v3);

    MAKE_BUF_USABLE(obuf, total_sz);

    encode_header_v3(obuf, CMD_FS2FC_START_TASK_RSP_V3, total_sz);

    int ret = 0;
    ret += encode_const_field(body->task_id, obuf);
    ret += encode_const_field(body->result, obuf);

    return ret;
}

int
decode_f2f_start_task_rsp_v3(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t total_sz = sizeof(proto_header)+sizeof(f2f_start_task_rsp_v3);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (h.size < total_sz || buffer_data_len(ibuf) < total_sz)
        return -2;
    f2f_start_task_rsp_v3 *body = (f2f_start_task_rsp_v3 *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->task_id = ntohl(body->task_id);
    body->result = ntohl(body->result);

    return 0;
}

int
encode_f2f_stop_task_v3(const f2f_stop_task_v3 * body, buffer * obuf)
{
    ASSERTR(body != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    uint32_t total_sz = sizeof(proto_header) + sizeof(f2f_stop_task_v3);

    MAKE_BUF_USABLE(obuf, total_sz);

    encode_header_v3(obuf, CMD_F2F_STOP_TASK_V3, total_sz);

    int ret = 0;
    ret += encode_const_field(body->task_id, obuf);
    ret += encode_const_field(body->result, obuf);

    return ret;
}

int
decode_f2f_stop_task_v3(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));

    size_t total_sz = sizeof(proto_header) + sizeof(f2f_stop_task_v3);
    proto_header h;
    if (0 != decode_header(ibuf, &h))
        return -1;

    if (h.size < total_sz || buffer_data_len(ibuf) < total_sz)
        return -2;

    f2f_stop_task_v3 *body = (f2f_stop_task_v3 *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->task_id = ntohl(body->task_id);
    body->result = ntohl(body->result);

    return 0;
}

int
encode_f2f_trans_data_piece_v3(const f2f_trans_data_v3 * body, buffer * obuf)
{
    ASSERTR(body != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    uint32_t total_sz = sizeof(proto_header) + sizeof(f2f_trans_data_v3)
        + body->payload_size;

    MAKE_BUF_USABLE(obuf, total_sz);

    encode_header_v3(obuf, CMD_FS2FC_STREAM_DATA_V3, total_sz);

    int ret = 0;
    ret += encode_const_field(body->task_id, obuf);
    ret += encode_const_field(body->payload_size, obuf);
    ret += encode_const_field((uint8_t *)body->payload, body->payload_size, obuf);

    return ret;
}

int
decode_f2f_trans_data_piece_v3(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t total_sz = sizeof(proto_header)+sizeof(f2f_trans_data_v3);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (h.size < total_sz || buffer_data_len(ibuf) < total_sz)
        return -2;
    f2f_trans_data_v3 *body = (f2f_trans_data_v3 *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->task_id = ntohl(body->task_id);
    body->payload_size = ntohl(body->payload_size);

    return 0;
}

int
decode_u2r_req_state(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t actual_sz = sizeof(proto_header)+sizeof(u2r_req_state);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (buffer_data_len(ibuf) < actual_sz || h.size < actual_sz)
        return -2;
    u2r_req_state *body = (u2r_req_state *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->streamid = ntohl(body->streamid);
    body->version = ntohl(body->version);
    body->user_id = util_ntohll(body->user_id);
    return 0;
}

int encode_u2r_rsp_state(const u2r_rsp_state * body, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(u2r_rsp_state);

    MAKE_BUF_USABLE(obuf, total_sz);
    encode_header(obuf, CMD_U2R_RSP_STATE, total_sz);
    uint32_t streamid = htonl(body->streamid);
    uint16_t result = htons(body->result);

    buffer_append_ptr(obuf, &streamid, sizeof(streamid));
    buffer_append_ptr(obuf, &result, sizeof(result));
    return 0;
}

int
decode_u2r_streaming(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t min_sz = sizeof(proto_header)+sizeof(u2r_streaming);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (buffer_data_len(ibuf) < min_sz)
        return -2;
    u2r_streaming *body = (u2r_streaming *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->streamid = ntohl(body->streamid);
    body->payload_size = ntohl(body->payload_size);
    size_t actual_sz = min_sz + body->payload_size;

    if (h.size < actual_sz || buffer_data_len(ibuf) < actual_sz)
        return -3;

    return 0;
}

int decode_u2r_req_state_v2(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t actual_sz = sizeof(proto_header)+sizeof(u2r_req_state_v2);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (buffer_data_len(ibuf) < actual_sz || h.size < actual_sz)
        return -2;
    u2r_req_state_v2 *body = (u2r_req_state_v2 *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->version = ntohl(body->version);
    body->user_id = util_ntohll(body->user_id);

    return 0;
}

int encode_u2r_rsp_state_v2(const u2r_rsp_state_v2 * body, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(u2r_rsp_state_v2);

    MAKE_BUF_USABLE(obuf, total_sz);
    encode_header(obuf, CMD_U2R_RSP_STATE_V2, total_sz);
    uint8_t streamid[16];
    memcpy(streamid, body->streamid, sizeof(streamid));
    uint16_t result = htons(body->result);

    buffer_append_ptr(obuf, &streamid, sizeof(streamid));
    buffer_append_ptr(obuf, &result, sizeof(result));
    return 0;
}

int decode_u2r_streaming_v2(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t min_sz = sizeof(proto_header)+sizeof(u2r_streaming_v2);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (buffer_data_len(ibuf) < min_sz)
        return -2;
    u2r_streaming_v2 *body = (u2r_streaming_v2 *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->payload_size = ntohl(body->payload_size);
    size_t actual_sz = min_sz + body->payload_size;

    if (h.size < actual_sz || buffer_data_len(ibuf) < actual_sz)
        return -3;
    return 0;
}

static unsigned long long net_util_htonll(unsigned long long val)
{
	return (((unsigned long long) htonl((int)((val << 32) >> 32))) <<
		32) | (unsigned int)htonl((int)(val >> 32));
}

int encode_u2r_req_state_v2(const u2r_req_state_v2 *body, buffer * obuf) {
	uint32_t total_sz = sizeof(proto_header)+sizeof(u2r_req_state_v2);
	if (obuf->capacity() < total_sz)
	{
		return -1;
	}

	encode_header(obuf, CMD_U2R_REQ_STATE_V2, total_sz);
	uint32_t version = htonl(body->version);
	uint64_t user_id = net_util_htonll(body->user_id);

	buffer_append_ptr(obuf,&version, sizeof(version));
	buffer_append_ptr(obuf,&(body->streamid), sizeof(body->streamid));
	buffer_append_ptr(obuf,&(user_id), sizeof(user_id));
	buffer_append_ptr(obuf,&(body->token), sizeof(body->token));
	buffer_append_ptr(obuf,&(body->payload_type), sizeof(uint8_t));

	return 0;
}
int decode_u2r_rsp_state_v2(u2r_rsp_state_v2 *rsp, buffer *ibuf) {
	size_t actual_sz = sizeof(proto_header)+sizeof(u2r_rsp_state_v2);
	proto_header h;

	int ret = decode_header(ibuf, &h);
	if (ret != 0)
	{
		return ret;
	}
	if (ibuf->data_len() < actual_sz || h.size < actual_sz)
	{
		return -3;
	}
	const u2r_rsp_state_v2 *body = (const u2r_rsp_state_v2 *)
		((const char*)(buffer_data_ptr(ibuf)) + sizeof(proto_header));

	memcpy(rsp->streamid, body->streamid, sizeof(rsp->streamid));
	rsp->result = ntohs(body->result);
  return 0;
}

int encode_u2r_streaming_v2(const u2r_streaming_v2 *body, buffer * obuf) {
	uint32_t total_sz = sizeof(proto_header)
		+sizeof(u2r_streaming_v2)
		+body->payload_size;

	if (obuf->capacity() < total_sz)
	{
		return -1;
	}
	encode_header(obuf, CMD_U2R_STREAMING_V2, total_sz);
	uint32_t payload_size = htonl(body->payload_size);

	buffer_append_ptr(obuf,&(body->streamid),sizeof(body->streamid));
	buffer_append_ptr(obuf,&(payload_size),sizeof(payload_size));
	buffer_append_ptr(obuf,&(body->payload_type),sizeof(body->payload_type));
	buffer_append_ptr(obuf,&(body->payload),body->payload_size);
  return 0;
}

int
decode_us2r_req_up(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t actual_sz = sizeof(proto_header)+sizeof(us2r_req_up);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (buffer_data_len(ibuf) < actual_sz || h.size < actual_sz)
        return -2;

    us2r_req_up *body = (us2r_req_up *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->seq_id = ntohl(body->seq_id);
    body->streamid = ntohl(body->streamid);
    body->user_id = util_ntohll(body->user_id);
    return 0;
}

int
encode_r2us_rsp_up(const r2us_rsp_up * body, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(r2us_rsp_up);

    MAKE_BUF_USABLE(obuf, total_sz);
    encode_header(obuf, CMD_R2US_RSP_UP, total_sz);
    uint32_t seqid = htonl(body->seq_id);
    uint32_t streamid = htonl(body->streamid);

    buffer_append_ptr(obuf, &seqid, sizeof(seqid));
    buffer_append_ptr(obuf, &streamid, sizeof(streamid));
    buffer_append_ptr(obuf, &body->result, sizeof(body->result));
    return 0;
}

int
encode_f2p_white_list_req(buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header);

    MAKE_BUF_USABLE(obuf, total_sz);
    encode_header(obuf, CMD_F2P_WHITE_LIST_REQ, total_sz);

    return 0;
}

int
encode_st2t_fpinfo_req(buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header);

    MAKE_BUF_USABLE(obuf, total_sz);
    encode_header(obuf, CMD_ST2T_FPINFO_REQ, total_sz);

    return 0;
}


int
encode_f2p_keepalive(const f2p_keepalive * body, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2p_keepalive)
        +body->stream_cnt * sizeof(forward_stream_status);

    MAKE_BUF_USABLE(obuf, total_sz);
    encode_header(obuf, CMD_F2P_KEEPALIVE, total_sz);
    uint32_t tmpu32 = htonl(body->listen_player_addr.ip);
    uint16_t tmpu16 = htons(body->listen_player_addr.port);
    int32_t tmp32;
    uint64_t tmpu64;

    buffer_append_ptr(obuf, &tmpu32, sizeof(uint32_t));
    buffer_append_ptr(obuf, &tmpu16, sizeof(uint16_t));
    tmpu32 = htonl(body->out_speed);
    buffer_append_ptr(obuf, &tmpu32, sizeof(uint32_t));
    tmpu32 = htonl(body->stream_cnt);
    buffer_append_ptr(obuf, &tmpu32, sizeof(uint32_t));
    uint32_t i = 0;
    forward_stream_status *fss = NULL;

    for (i = 0; i < body->stream_cnt; i++) {
        fss = (forward_stream_status *)(body->stream_status + i);
        tmpu32 = htonl(fss->streamid);
        buffer_append_ptr(obuf, &tmpu32, sizeof(uint32_t));
        tmpu32 = htonl(fss->player_cnt);
        buffer_append_ptr(obuf, &tmpu32, sizeof(uint32_t));
        tmpu32 = htonl(fss->forward_cnt);
        buffer_append_ptr(obuf, &tmpu32, sizeof(uint32_t));
        tmp32 = htonl(fss->last_ts);
        buffer_append_ptr(obuf, &tmp32, sizeof(int32_t));
        tmpu64 = util_htonll(fss->last_block_seq);
        buffer_append_ptr(obuf, &tmpu64, sizeof(uint64_t));
    }
    return 0;
}

int32_t decode_p2f_inf_stream_v2(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t min_sz = sizeof(proto_header)+sizeof(p2f_inf_stream_v2);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (buffer_data_len(ibuf) < min_sz)
        return -2;

    p2f_inf_stream_v2 *body = (p2f_inf_stream_v2 *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->cnt = ntohs(body->cnt);
    size_t actual_sz = min_sz + body->cnt * sizeof(stream_info);


    if (buffer_data_len(ibuf) < actual_sz || h.size < actual_sz)
        return -3;
    int i = 0;

    for (i = 0; i < body->cnt; i++)
    {
        body->stream_infos[i].streamid = ntohl(body->stream_infos[i].streamid);
        body->stream_infos[i].start_time.tv_sec
            = util_ntohll(body->stream_infos[i].start_time.tv_sec);
        body->stream_infos[i].start_time.tv_usec
            = util_ntohll(body->stream_infos[i].start_time.tv_usec);
    }

    return 0;
}

int
decode_p2f_start_stream_v2(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t actual_sz = sizeof(proto_header)+sizeof(p2f_start_stream_v2);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (buffer_data_len(ibuf) < actual_sz || h.size < actual_sz)
        return -2;

    p2f_start_stream_v2 *body = (p2f_start_stream_v2 *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->streamid = ntohl(body->streamid);
    body->start_time.tv_sec = util_ntohll(body->start_time.tv_sec);
    body->start_time.tv_usec = util_ntohll(body->start_time.tv_usec);

    return 0;
}

int decode_p2f_inf_stream(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t min_sz = sizeof(proto_header)+sizeof(p2f_inf_stream);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (buffer_data_len(ibuf) < min_sz)
        return -2;

    p2f_inf_stream *body = (p2f_inf_stream *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->cnt = ntohs(body->cnt);
    size_t actual_sz = min_sz + body->cnt * sizeof(uint32_t);

    if (buffer_data_len(ibuf) < actual_sz || h.size < actual_sz)
        return -3;
    int i = 0;

    for (i = 0; i < body->cnt; i++)
        body->streamids[i] = ntohl(body->streamids[i]);
    return 0;
}

int
decode_p2f_start_stream(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t actual_sz = sizeof(proto_header)+sizeof(p2f_start_stream);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (buffer_data_len(ibuf) < actual_sz || h.size < actual_sz)
        return -2;

    p2f_start_stream *body = (p2f_start_stream *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->streamid = ntohl(body->streamid);
    return 0;
}

int
decode_p2f_close_stream(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    size_t actual_sz = sizeof(proto_header)+sizeof(p2f_close_stream);
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    if (buffer_data_len(ibuf) < actual_sz || h.size < actual_sz)
        return -2;

    p2f_close_stream *body = (p2f_close_stream *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->streamid = ntohl(body->streamid);
    return 0;
}

int
encode_r2p_keepalive(const r2p_keepalive * body, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(r2p_keepalive)
        +body->stream_cnt * sizeof(receiver_stream_status);

    MAKE_BUF_USABLE(obuf, total_sz);
    uint32_t uint32 = 0;
    uint16_t uint16 = 0;
    uint64_t uint64 = 0;
    uint32_t i = 0;

    encode_header(obuf, CMD_R2P_KEEPALIVE, total_sz);
    uint32 = htonl(body->listen_uploader_addr.ip);
    uint16 = htons(body->listen_uploader_addr.port);
    buffer_append_ptr(obuf, &uint32, sizeof(uint32_t));
    buffer_append_ptr(obuf, &uint16, sizeof(uint16_t));
    uint32 = htonl(body->outbound_speed);
    buffer_append_ptr(obuf, &uint32, sizeof(uint32_t));
    uint32 = htonl(body->inbound_speed);
    buffer_append_ptr(obuf, &uint32, sizeof(uint32_t));
    uint32 = htonl(body->stream_cnt);
    buffer_append_ptr(obuf, &uint32, sizeof(uint32_t));
    for (i = 0; i < body->stream_cnt; i++) {
        uint32 = htonl(body->streams[i].streamid);
        buffer_append_ptr(obuf, &uint32, sizeof(uint32_t));
        uint32 = htonl(body->streams[i].forward_cnt);
        buffer_append_ptr(obuf, &uint32, sizeof(uint32_t));
        uint32 = htonl(body->streams[i].last_ts);
        buffer_append_ptr(obuf, &uint32, sizeof(uint32_t));
        uint64 = util_htonll(body->streams[i].block_seq);
        buffer_append_ptr(obuf, &uint64, sizeof(uint64_t));

        DBG("encode_r2p_keepalive: %032x", body->streams[i].streamid);
    }
    return 0;
}

int
encode_f2t_addr_req(const f2t_addr_req * req, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2t_addr_req);

    MAKE_BUF_USABLE(obuf, total_sz);
    encode_header(obuf, CMD_F2T_ADDR_REQ, sizeof(proto_header)
        +sizeof(f2t_addr_req));
    uint32_t streamid = htonl(req->streamid);
    uint32_t ip = htonl(req->ip);
    uint16_t port = htons(req->port);
    uint16_t asn = htons(req->asn);
    uint16_t region = htons(req->region);
    uint16_t level = htons(req->level);

    buffer_append_ptr(obuf, &streamid, sizeof(streamid));
    buffer_append_ptr(obuf, &ip, sizeof(ip));
    buffer_append_ptr(obuf, &port, sizeof(port));
    buffer_append_ptr(obuf, &asn, sizeof(asn));
    buffer_append_ptr(obuf, &region, sizeof(region));
    buffer_append_ptr(obuf, &level, sizeof(level));
    return 0;
}

int
decode_f2t_addr_req(f2t_addr_req * req, buffer * ibuf)
{
    if (buffer_data_len(ibuf) < sizeof(proto_header)+sizeof(f2t_addr_req))
        return -1;
    uint16_t cmd = ntohs(((proto_header *)buffer_data_ptr(ibuf))->cmd);
    uint32_t size = ntohl(((proto_header *)buffer_data_ptr(ibuf))->size);

    if (CMD_F2T_ADDR_REQ != cmd)
        return -1;
    buffer_eat(ibuf, sizeof(proto_header));

    const f2t_addr_req *r = (const f2t_addr_req *)(buffer_data_ptr(ibuf));

    req->streamid = ntohl(r->streamid);
    req->ip = ntohl(r->ip);
    req->port = ntohs(r->port);
    req->asn = ntohs(r->asn);
    req->region = ntohs(r->region);
    req->level = ntohs(r->level);

    //buffer_eat (ibuf, sizeof (f2t_addr_req));
    buffer_eat(ibuf, size - sizeof(proto_header));
    return 0;
}

int
encode_f2t_addr_rsp(const f2t_addr_rsp * rsp, buffer * obuf)
{
    encode_header(obuf, CMD_F2T_ADDR_RSP, sizeof(proto_header)
        +sizeof(f2t_addr_rsp));

    //uint64_t id = util_htonll(req->fid);
    uint32_t ip = htonl(rsp->ip);
    uint16_t port = htons(rsp->port);
    uint16_t result = htons(rsp->result);
    uint16_t level = htons(rsp->level);

    //buffer_append_ptr(obuf, &id, sizeof(uint64_t));
    buffer_append_ptr(obuf, &ip, sizeof(ip));
    buffer_append_ptr(obuf, &port, sizeof(port));
    buffer_append_ptr(obuf, &result, sizeof(result));
    buffer_append_ptr(obuf, &level, sizeof(level));

    return 0;
}

/*
* decode f2t_addr_rsp
*/
int
decode_f2t_addr_rsp_o(buffer * ibuf)
{
    proto_header h;

    if (0 != decode_header(ibuf, &h))
        return -1;
    assert(CMD_F2T_ADDR_RSP == h.cmd);
    f2t_addr_rsp *body = (f2t_addr_rsp *)(buffer_data_ptr(ibuf)
        + sizeof(proto_header));

    body->ip = ntohl(body->ip);
    body->port = ntohs(body->port);
    body->result = ntohs(body->result);
    body->level = ntohs(body->level);
    return 0;
}

/*
* decode f2t_addr_rsp
* afterwords, ibuf will be eaten
*/
int
decode_f2t_addr_rsp(f2t_addr_rsp * rsp, buffer * ibuf)
{
    if (buffer_data_len(ibuf) < sizeof(proto_header)+sizeof(f2t_addr_rsp))
        return -1;
    uint32_t size = ntohl(((proto_header *)buffer_data_ptr(ibuf))->size);

    buffer_eat(ibuf, sizeof(proto_header));

    const f2t_addr_rsp *r = (const f2t_addr_rsp *)(buffer_data_ptr(ibuf));

    rsp->ip = ntohl(r->ip);
    rsp->port = ntohs(r->port);
    rsp->result = ntohs(r->result);
    rsp->level = ntohs(r->level);

    //buffer_eat (ibuf, sizeof (f2t_addr_rsp));
    buffer_eat(ibuf, size - sizeof(proto_header));
    return 0;
}

int
encode_f2t_update_stream_req(const f2t_update_stream_req * body,
buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2t_update_stream_req);

    MAKE_BUF_USABLE(obuf, total_sz);
    encode_header(obuf, CMD_F2T_UPDATE_STREAM_REQ, sizeof(proto_header)
        +sizeof(f2t_update_stream_req));

    uint16_t cmd = htons(body->cmd);
    uint32_t streamid = htonl(body->streamid);
    uint16_t level = htons(body->level);

    buffer_append_ptr(obuf, &cmd, sizeof(cmd));
    buffer_append_ptr(obuf, &streamid, sizeof(streamid));
    buffer_append_ptr(obuf, &level, sizeof(level));

    return 0;
}

int
decode_f2t_update_stream_req(f2t_update_stream_req * body, buffer * ibuf)
{
    if (buffer_data_len(ibuf) < sizeof(proto_header)
        +sizeof(f2t_update_stream_req))
        return -1;
    uint16_t cmd = ntohs(((proto_header *)buffer_data_ptr(ibuf))->cmd);
    uint32_t size = ntohl(((proto_header *)buffer_data_ptr(ibuf))->size);

    if (CMD_F2T_UPDATE_STREAM_REQ != cmd)
        return -1;

    buffer_eat(ibuf, sizeof(proto_header));

    const f2t_update_stream_req *map_ptr =
        (const f2t_update_stream_req *)buffer_data_ptr(ibuf);
    body->cmd = ntohs(map_ptr->cmd);
    body->streamid = ntohl(map_ptr->streamid);
    body->level = ntohs(map_ptr->level);

    //buffer_eat (ibuf, sizeof (f2t_update_stream_req));
    buffer_eat(ibuf, size - sizeof(proto_header));
    return 0;
}

int
encode_f2t_keep_alive_req(const f2t_keep_alive_req * req, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2t_keep_alive_req);

    MAKE_BUF_USABLE(obuf, total_sz);
    encode_header(obuf, CMD_F2T_KEEP_ALIVE_REQ, sizeof(proto_header)
        +sizeof(f2t_keep_alive_req));
    uint64_t id = util_htonll(req->fid);

    buffer_append_ptr(obuf, &id, sizeof(id));
    return 0;
}

int
decode_f2t_keep_alive_req(f2t_keep_alive_req * req, buffer * ibuf)
{
    if (buffer_data_len(ibuf) < sizeof(proto_header)
        +sizeof(f2t_keep_alive_req))
        return -1;
    uint16_t cmd = ntohs(((proto_header *)buffer_data_ptr(ibuf))->cmd);
    uint32_t size = ntohl(((proto_header *)buffer_data_ptr(ibuf))->size);

    if (CMD_F2T_KEEP_ALIVE_REQ != cmd)
        return -1;

    buffer_eat(ibuf, sizeof(proto_header));

    const f2t_keep_alive_req *r = (const f2t_keep_alive_req *)buffer_data_ptr(
        ibuf);
    req->fid = util_ntohll(r->fid);

    //buffer_eat (ibuf, sizeof (f2t_keep_alive_req));
    buffer_eat(ibuf, size - sizeof(proto_header));
    return 0;
}

int
encode_f2t_keep_alive_rsp(const f2t_keep_alive_rsp * rsp, buffer * obuf)
{
    encode_header(obuf, CMD_F2T_KEEP_ALIVE_RSP, sizeof(proto_header)
        +sizeof(f2t_keep_alive_rsp));
    uint16_t result = htons(rsp->result);

    buffer_append_ptr(obuf, &result, sizeof(result));
    return 0;
}

int
decode_f2t_keep_alive_rsp_o(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >=
        sizeof(proto_header)+sizeof(f2t_keep_alive_rsp));
    f2t_keep_alive_rsp *r = (f2t_keep_alive_rsp *)buffer_data_ptr(ibuf);

    r->result = ntohs(r->result);
    return 0;
}

int
decode_f2t_keep_alive_rsp(f2t_keep_alive_rsp * rsp, buffer * ibuf)
{
    if (buffer_data_len(ibuf) < sizeof(proto_header)
        +sizeof(f2t_keep_alive_rsp))
        return -1;
    uint32_t size = ntohl(((proto_header *)buffer_data_ptr(ibuf))->size);

    buffer_eat(ibuf, sizeof(proto_header));

    const f2t_keep_alive_rsp *r = (const f2t_keep_alive_rsp *)buffer_data_ptr(
        ibuf);
    rsp->result = ntohs(r->result);

    //buffer_eat (ibuf, sizeof (f2t_keep_alive_rsp));
    buffer_eat(ibuf, size - sizeof(proto_header));
    return 0;
}

int
encode_f2t_update_rsp(const f2t_update_stream_rsp * rsp, buffer * obuf)
{
    encode_header(obuf, CMD_F2T_UPDATE_STREAM_RSP, sizeof(proto_header)
        +sizeof(f2t_update_stream_rsp));
    uint16_t result = htons(rsp->result);

    buffer_append_ptr(obuf, &result, sizeof(result));
    return 0;
}

int
decode_f2t_update_rsp_o(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >=
        sizeof(proto_header)+sizeof(f2t_update_stream_rsp));

    f2t_update_stream_rsp *r = (f2t_update_stream_rsp *)buffer_data_ptr(ibuf);

    r->result = ntohs(r->result);
    return 0;
}

int
decode_f2t_update_rsp(f2t_update_stream_rsp * rsp, buffer * ibuf)
{
    if (buffer_data_len(ibuf) < sizeof(proto_header)+sizeof(f2t_update_stream_rsp))
        return -1;
    uint32_t size = ntohl(((proto_header *)buffer_data_ptr(ibuf))->size);

    buffer_eat(ibuf, sizeof(proto_header));

    const f2t_update_stream_rsp *r = (const f2t_update_stream_rsp *)buffer_data_ptr(ibuf);

    rsp->result = ntohs(r->result);

    //buffer_eat (ibuf, sizeof (f2t_update_rsp));
    buffer_eat(ibuf, size - sizeof(proto_header));
    return 0;
}

int
encode_f2t_register_req(const f2t_register_req_t * req, buffer * obuf)
{
    uint32_t total_sz = sizeof(proto_header)+sizeof(f2t_register_req_t);

    MAKE_BUF_USABLE(obuf, total_sz);
    encode_header(obuf, CMD_F2T_REGISTER_REQ, sizeof(proto_header)
        +sizeof(f2t_register_req_t));
    uint16_t port = htons(req->port);
    uint32_t ip = htonl(req->ip);
    uint16_t asn = htons(req->asn);
    uint16_t region = htons(req->region);

    buffer_append_ptr(obuf, &port, sizeof(port));
    buffer_append_ptr(obuf, &ip, sizeof(ip));
    buffer_append_ptr(obuf, &asn, sizeof(asn));
    buffer_append_ptr(obuf, &region, sizeof(region));
    return 0;
}

int
decode_f2t_register_req(f2t_register_req_t * req, buffer * ibuf)
{
    if (buffer_data_len(ibuf) < sizeof(proto_header)
        +sizeof(f2t_register_req_t))
        return -1;
    uint16_t cmd = ntohs(((proto_header *)buffer_data_ptr(ibuf))->cmd);
    uint32_t size = ntohl(((proto_header *)buffer_data_ptr(ibuf))->size);

    if (CMD_F2T_REGISTER_REQ != cmd)
        return -1;

    buffer_eat(ibuf, sizeof(proto_header));

    const f2t_register_req_t *r = (const f2t_register_req_t *)buffer_data_ptr(
        ibuf);
    req->port = ntohs(r->port);
    req->ip = ntohl(r->ip);
    req->asn = ntohs(r->asn);
    req->region = ntohs(r->region);

    //buffer_eat (ibuf, sizeof (f2t_register_req_t));
    buffer_eat(ibuf, size - sizeof(proto_header));
    return 0;
}

int
encode_f2t_register_rsp(const f2t_register_rsp_t * rsp, buffer * obuf)
{
    encode_header(obuf, CMD_F2T_REGISTER_RSP, sizeof(proto_header)
        +sizeof(f2t_register_rsp_t));
    uint16_t result = htons(rsp->result);

    buffer_append_ptr(obuf, &result, sizeof(result));
    return 0;
}

int
decode_f2t_register_rsp_o(buffer * ibuf)
{
    assert(buffer_data_len(ibuf) >=
        sizeof(proto_header)+sizeof(f2t_register_rsp_t));

    f2t_register_rsp_t *r = (f2t_register_rsp_t *)buffer_data_ptr(ibuf);

    r->result = ntohs(r->result);
    return 0;
}

int
decode_f2t_register_rsp(f2t_register_rsp_t * rsp, buffer * ibuf)
{
    if (buffer_data_len(ibuf) < sizeof(proto_header)
        +sizeof(f2t_register_rsp_t))
        return -1;
    uint32_t size = ntohl(((proto_header *)buffer_data_ptr(ibuf))->size);

    buffer_eat(ibuf, sizeof(proto_header));

    const f2t_register_rsp_t *r = (const f2t_register_rsp_t *)buffer_data_ptr(
        ibuf);
    rsp->result = ntohs(r->result);

    //buffer_eat (ibuf, sizeof (f2t_register_rsp_t));
    buffer_eat(ibuf, size - sizeof(proto_header));
    return 0;
}

//tracker<--->forward v2

int encode_f2t_addr_req_v2(const f2t_addr_req_v2 * packet, buffer * obuf)
{
    ASSERTR(packet != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    MAKE_BUF_USABLE(obuf, sizeof(f2t_addr_req_v2));

    int ret = 0;
    ret += encode_const_field(packet->streamid, obuf);
    ret += encode_const_field(packet->ip, obuf);
    ret += encode_const_field(packet->port, obuf);
    ret += encode_const_field(packet->asn, obuf);
    ret += encode_const_field(packet->region, obuf);
    ret += encode_const_field(packet->level, obuf);

    return ret;
}

int encode_f2t_addr_rsp_v2(const f2t_addr_rsp_v2 * packet, buffer * obuf)
{
    ASSERTR(packet != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    MAKE_BUF_USABLE(obuf, sizeof(f2t_addr_rsp_v2));

    int ret = 0;
    ret += encode_const_field(packet->ip, obuf);
    ret += encode_const_field(packet->port, obuf);
    ret += encode_const_field(packet->result, obuf);
    ret += encode_const_field(packet->level, obuf);

    return ret;
}

int encode_f2t_update_stream_req_v2(const f2t_update_stream_req_v2 * packet, buffer * obuf)
{
    ASSERTR(packet != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    MAKE_BUF_USABLE(obuf, sizeof(f2t_update_stream_req_v2));

    int ret = 0;
    ret += encode_const_field(packet->cmd, obuf);
    ret += encode_const_field(packet->streamid, obuf);
    ret += encode_const_field(packet->level, obuf);

    return ret;
}

int encode_f2t_update_rsp_v2(const f2t_update_stream_rsp_v2 * packet, buffer * obuf)
{
    ASSERTR(packet != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    MAKE_BUF_USABLE(obuf, sizeof(f2t_update_stream_rsp_v2));

    int ret = 0;
    ret += encode_const_field(packet->result, obuf);

    return ret;
}

int encode_f2t_keep_alive_req_v2(const f2t_keep_alive_req_v2 * packet, buffer * obuf)
{
    ASSERTR(packet != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    MAKE_BUF_USABLE(obuf, sizeof(f2t_keep_alive_req_v2));

    int ret = 0;
    ret += encode_const_field(packet->fid, obuf);

    return ret;
}

int encode_f2t_keep_alive_rsp_v2(const f2t_keep_alive_rsp_v2 * packet, buffer * obuf)
{
    ASSERTR(packet != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    MAKE_BUF_USABLE(obuf, sizeof(f2t_keep_alive_rsp_v2));

    int ret = 0;
    ret += encode_const_field(packet->result, obuf);

    return ret;
}

int encode_f2t_register_req_v2(const f2t_register_req_v2 * packet, buffer * obuf)
{
    ASSERTR(packet != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    MAKE_BUF_USABLE(obuf, sizeof(f2t_register_req_v2));

    int ret = 0;
    ret += encode_const_field(packet->port, obuf);
    ret += encode_const_field(packet->ip, obuf);
    ret += encode_const_field(packet->asn, obuf);
    ret += encode_const_field(packet->region, obuf);

    return ret;
}

int encode_f2t_register_rsp_v2(const f2t_register_rsp_v2 * packet, buffer * obuf)
{
    ASSERTR(packet != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    MAKE_BUF_USABLE(obuf, sizeof(f2t_register_rsp_v2));

    int ret = 0;
    ret += encode_const_field(packet->result, obuf);

    return ret;
}

bool decode_f2t_addr_req_v2(f2t_addr_req_v2 * packet, buffer * ibuf)
{
    ASSERTR(packet != NULL, false);
    ASSERTR(ibuf != NULL, false);

    if (buffer_data_len(ibuf) < sizeof(f2t_addr_req_v2))
        return false;

    const f2t_addr_req_v2* r = (const f2t_addr_req_v2 *)(buffer_data_ptr(ibuf));
    ASSERTR(r != NULL, false);

    packet->streamid = ntohl(r->streamid);
    packet->ip = ntohl(r->ip);
    packet->port = ntohs(r->port);
    packet->asn = ntohs(r->asn);
    packet->region = ntohs(r->region);
    packet->level = ntohs(r->level);

    return true;
}

bool decode_f2t_addr_rsp_v2(f2t_addr_rsp_v2 * packet, buffer * ibuf)
{
    ASSERTR(packet != NULL, false);
    ASSERTR(ibuf != NULL, false);

    if (buffer_data_len(ibuf) < sizeof(f2t_addr_rsp_v2))
        return false;

    const f2t_addr_rsp_v2* r = (const f2t_addr_rsp_v2 *)(buffer_data_ptr(ibuf));
    ASSERTR(r != NULL, false);

    packet->ip = ntohl(r->ip);
    packet->port = ntohs(r->port);
    packet->result = ntohs(r->result);
    packet->level = ntohs(r->level);

    return true;
}

bool decode_f2t_update_stream_req_v2(f2t_update_stream_req_v2 * packet, buffer * ibuf)
{
    ASSERTR(packet != NULL, false);
    ASSERTR(ibuf != NULL, false);

    if (buffer_data_len(ibuf) < sizeof(f2t_update_stream_req_v2))
        return false;

    const f2t_update_stream_req_v2* r = (const f2t_update_stream_req_v2 *)(buffer_data_ptr(ibuf));
    ASSERTR(r != NULL, false);

    packet->cmd = ntohs(r->cmd);
    packet->streamid = ntohl(r->streamid);
    packet->level = ntohs(r->level);

    return true;
}

bool decode_f2t_keep_alive_req_v2(f2t_keep_alive_req_v2 * packet, buffer * ibuf)
{
    ASSERTR(packet != NULL, false);
    ASSERTR(ibuf != NULL, false);

    if (buffer_data_len(ibuf) < sizeof(f2t_keep_alive_req_v2))
        return false;

    const f2t_keep_alive_req_v2* r = (const f2t_keep_alive_req_v2 *)(buffer_data_ptr(ibuf));
    ASSERTR(r != NULL, false);

    packet->fid = util_ntohll(r->fid);

    return true;
}

bool decode_f2t_keep_alive_rsp_v2(f2t_keep_alive_rsp_v2 * packet, buffer * ibuf)
{
    ASSERTR(packet != NULL, false);
    ASSERTR(ibuf != NULL, false);

    if (buffer_data_len(ibuf) < sizeof(f2t_keep_alive_rsp_v2))
        return false;

    const f2t_keep_alive_rsp_v2* r = (const f2t_keep_alive_rsp_v2 *)(buffer_data_ptr(ibuf));
    ASSERTR(r != NULL, false);

    packet->result = ntohs(r->result);

    return true;
}

bool decode_f2t_update_rsp_v2(f2t_update_stream_rsp_v2 * packet, buffer * ibuf)
{
    ASSERTR(packet != NULL, false);
    ASSERTR(ibuf != NULL, false);

    if (buffer_data_len(ibuf) < sizeof(f2t_update_stream_rsp_v2))
        return false;

    const f2t_update_stream_rsp_v2* r = (const f2t_update_stream_rsp_v2 *)(buffer_data_ptr(ibuf));
    ASSERTR(r != NULL, false);

    packet->result = ntohs(r->result);

    return true;
}

bool decode_f2t_register_req_v2(f2t_register_req_v2 * packet, buffer * ibuf)
{
    ASSERTR(packet != NULL, false);
    ASSERTR(ibuf != NULL, false);

    if (buffer_data_len(ibuf) < sizeof(f2t_register_req_v2))
        return false;

    const f2t_register_req_v2* r = (const f2t_register_req_v2 *)(buffer_data_ptr(ibuf));
    ASSERTR(r != NULL, false);

    packet->port = ntohs(r->port);
    packet->ip = ntohl(r->ip);
    packet->asn = ntohs(r->asn);
    packet->region = ntohs(r->region);

    return true;
}

bool decode_f2t_register_rsp_v2(f2t_register_rsp_v2 * packet, buffer * ibuf)
{
    ASSERTR(packet != NULL, false);
    ASSERTR(ibuf != NULL, false);

    if (buffer_data_len(ibuf) < sizeof(f2t_register_rsp_v2))
        return false;

    const f2t_register_rsp_v2* r = (const f2t_register_rsp_v2 *)(buffer_data_ptr(ibuf));
    ASSERTR(r != NULL, false);

    packet->result = ntohs(r->result);

    return true;
}

//tracker<--->forward v3 (support long stream id)

int encode_f2t_addr_req_v3(const f2t_addr_req_v3 * packet, buffer * obuf)
{
    ASSERTR(packet != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    MAKE_BUF_USABLE(obuf, sizeof(f2t_addr_req_v3));

    int ret = 0;
    ret += encode_const_field((uint8_t *)packet->streamid, STREAM_ID_LEN, obuf);
    ret += encode_const_field(packet->ip, obuf);
    ret += encode_const_field(packet->port, obuf);
    ret += encode_const_field(packet->asn, obuf);
    ret += encode_const_field(packet->region, obuf);
    ret += encode_const_field(packet->level, obuf);

    return ret;
}

int encode_f2t_addr_rsp_v3(const f2t_addr_rsp_v3 * packet, buffer * obuf)
{
    return encode_f2t_addr_rsp_v2((f2t_addr_rsp_v2 *)packet, obuf);
}

int encode_f2t_update_stream_req_v3(const f2t_update_stream_req_v3 * packet, buffer * obuf)
{
    ASSERTR(packet != NULL, -1);
    ASSERTR(obuf != NULL, -1);

    MAKE_BUF_USABLE(obuf, sizeof(f2t_update_stream_req_v3));

    int ret = 0;
    ret += encode_const_field(packet->cmd, obuf);
    ret += encode_const_field((uint8_t *)packet->streamid, STREAM_ID_LEN, obuf);
    ret += encode_const_field(packet->level, obuf);

    return ret;
}

int encode_f2t_update_stream_rsp_v3(const f2t_update_stream_rsp_v3 * packet, buffer * obuf)
{
    return encode_f2t_update_rsp_v2((f2t_update_stream_rsp_v2 *)packet, obuf);
}

int encode_f2t_keep_alive_req_v3(const f2t_keep_alive_req_v3 * packet, buffer * obuf)
{
    return encode_f2t_keep_alive_req_v2((f2t_keep_alive_req_v2 *)packet, obuf);
}

int encode_f2t_keep_alive_rsp_v3(const f2t_keep_alive_rsp_v3 * packet, buffer * obuf)
{
    return encode_f2t_keep_alive_rsp_v2((f2t_keep_alive_rsp_v2 *)packet, obuf);
}

int encode_f2t_server_stat_rsp(const f2t_server_stat_rsp * packet, buffer * obuf)
{
    return encode_f2t_keep_alive_rsp_v2((f2t_keep_alive_rsp_v2 *)packet, obuf);
}

int encode_f2t_register_req_v3(const f2t_register_req_v3 * packet, buffer * obuf)
{
    return encode_f2t_register_req_v2((f2t_register_req_v2 *)packet, obuf);
}

int encode_f2t_register_rsp_v3(const f2t_register_rsp_v3 * packet, buffer * obuf)
{
    return encode_f2t_register_rsp_v2((f2t_register_rsp_v2 *)packet, obuf);
}

bool decode_f2t_addr_req_v3(f2t_addr_req_v3 * packet, buffer * ibuf)
{
    ASSERTR(packet != NULL, false);
    ASSERTR(ibuf != NULL, false);

    if (buffer_data_len(ibuf) < sizeof(f2t_addr_req_v3))
        return false;

    const f2t_addr_req_v3* r = (const f2t_addr_req_v3 *)(buffer_data_ptr(ibuf));
    ASSERTR(r != NULL, false);

    memcpy(packet->streamid, r->streamid, sizeof(packet->streamid));
    packet->ip = ntohl(r->ip);
    packet->port = ntohs(r->port);
    packet->asn = ntohs(r->asn);
    packet->region = ntohs(r->region);
    packet->level = ntohs(r->level);

    return true;
}

bool decode_f2t_addr_rsp_v3(f2t_addr_rsp_v3 * packet, buffer * ibuf)
{
    return decode_f2t_addr_rsp_v2((f2t_addr_rsp_v2 *)packet, ibuf);
}

bool decode_f2t_update_stream_req_v3(f2t_update_stream_req_v3 * packet, buffer * ibuf)
{
    ASSERTR(packet != NULL, false);
    ASSERTR(ibuf != NULL, false);

    if (buffer_data_len(ibuf) < sizeof(f2t_update_stream_req_v3))
        return false;

    const f2t_update_stream_req_v3* r = (const f2t_update_stream_req_v3 *)(buffer_data_ptr(ibuf));
    ASSERTR(r != NULL, false);

    packet->cmd = ntohs(r->cmd);
    memcpy(packet->streamid, r->streamid, sizeof(packet->streamid));
    packet->level = ntohs(r->level);

    return true;
}

bool decode_f2t_update_stream_rsp_v3(f2t_update_stream_rsp_v3 * packet, buffer * ibuf)
{
    return decode_f2t_update_rsp_v2((f2t_update_stream_rsp_v2 *)packet, ibuf);
}

bool decode_f2t_keep_alive_req_v3(f2t_keep_alive_req_v3 * packet, buffer * ibuf)
{
    return decode_f2t_keep_alive_req_v2((f2t_keep_alive_req_v2 *)packet, ibuf);
}

bool decode_f2t_keep_alive_rsp_v3(f2t_keep_alive_rsp_v3 * packet, buffer * ibuf)
{
    return decode_f2t_keep_alive_rsp_v2((f2t_keep_alive_rsp_v2 *)packet, ibuf);
}

bool decode_f2t_server_stat_rsp(f2t_server_stat_rsp * packet, buffer * ibuf)
{
    return decode_f2t_keep_alive_rsp_v2((f2t_keep_alive_rsp_v2 *)packet, ibuf);
}

bool decode_f2t_register_req_v3(f2t_register_req_v3 * packet, buffer * ibuf)
{
    return decode_f2t_register_req_v2((f2t_register_req_v2 *)packet, ibuf);
}

bool decode_f2t_register_rsp_v3(f2t_register_rsp_v3 * packet, buffer * ibuf)
{
    return decode_f2t_register_rsp_v2((f2t_register_rsp_v2 *)packet, ibuf);
}


int decode_t2st_fpinfo_rsp(buffer *ibuf)
{
    assert(buffer_data_len(ibuf) >= sizeof(proto_header));
    proto_header h;

    if (0 != decode_header(ibuf, &h))
    {
        return -1;
    }

    if (buffer_data_len(ibuf) <  h.size)
    {
        return -2;
    }

    return 0;
}
