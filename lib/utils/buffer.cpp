/*
 * a Buffer implementation
 * author: hechao@youku.com
 * create: 2014/3/5
 * 
 */

#include "buffer.hpp"
#include "memory.h"

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "utils/logging.h"

using namespace lcdn;
using namespace lcdn::base;

Buffer::Buffer(uint32_t cap)
{
    _start = 0;
    _end = 0;
    _size = cap;
    _max = cap;
#define BUFFER_PIECE_SIZE 64
    _size += BUFFER_PIECE_SIZE - (_size % BUFFER_PIECE_SIZE);
    _max = _size;
#undef BUFFER_PIECE_SIZE
    // there is no need to check _ptr is not a null
    _ptr = (char *)calloc(1, _size);
}

Buffer::~Buffer()
{
    if (_ptr)
    {
        free(_ptr);
        _ptr = NULL;
    }
}

int Buffer::append(const Buffer * src)
{
    assert(NULL != src);
    assert(NULL != src->_ptr);
    //return append_ptr(src->_ptr + src->_start, src->_end - src->_start);
    return append_ptr(src->data_ptr(), src->data_len());
}

int Buffer::append(const Buffer *src, size_t src_len)
{
    size_t len = src_len < src->data_len() ?
        src_len : src->data_len();
    if (len > free_size())
    {
        return 0;
    }

    return append_ptr(src->data_ptr(), len);
}

int Buffer::append_ptr(const void *src, size_t src_len)
{
    assert(NULL != src);

    if (_end - _start + src_len > _size)
    {
        return 0;
    }

    if (_size - _end < src_len)
    {
        adjust();
    }

    memmove(_ptr + _end, src, src_len);
    _end += src_len;
    return src_len;
}

int Buffer::append_byte(unsigned char byte)
{
    return append_ptr(&byte, 1);
}

int Buffer::read_fd(int fd)
{
    //return read_fd_max(fd, 1024 * 1024 * 1024);
    return read_fd_max(fd, free_size());
}

int Buffer::read_fd_max(int fd, size_t max)
{
    int toread = 0, r;

    if (fd <= 0)
    {
        return -1;
    }
    if (ioctl(fd, FIONREAD, &toread))
    {
        if (errno != EAGAIN && errno != EINTR) {
            return -1;
        }
        else
            return 0;
    }

    // try to detect whether it's eof
    if (toread == 0)
    {
        toread = 1;
    }
    toread = (toread <= (int)max) ? toread : (int)max;
    if (_max > 0)
    {
      toread = (toread <= ((int)_max - (int)data_len())) ? toread : ((int)_max - (int)data_len());
    }

    if (toread >= (int)free_size())
    {
        return -2; // expand receive Buffer 
    }

    toread = toread < (int)capacity() ? toread : (int)capacity();

    //r = read(fd, _ptr + _end, toread);
    r = read(fd, free_ptr(), toread);
    if ((r == -1 && (errno != EINTR && errno != EAGAIN)) || r == 0)
    {
        return -1;
    }

    if (r > 0)
    {
        _end += r;
    }
    return r;
}

int Buffer::write_fd(int fd)
{
    return Buffer::write_fd_max(fd, data_len());
}

int Buffer::write_fd_max(int fd, size_t max)
{
    int ret = 0, total = 0;
    size_t end = _start + _max > _end ? _end : _start + max;

    while (end - _start > 0)
    {
        ret = write(fd, data_ptr(), end - _start);
        if (ret < 0)
        {
            if ((ret == -1 && (EINTR != errno && EAGAIN != errno)))
                return -1;
        }
        else if (ret == 0)
        {
            break;
        }
        else
        {
            total += ret;
            _start += ret;
        }
    }
    return total;
}

int Buffer::write_buffer(Buffer *dst)
{
    return dst->append(this);
}

int Buffer::write_buffer(void *dst, size_t dst_len)
{
    size_t len = dst_len < data_len() ?
        dst_len : data_len();

    memmove(dst, data_ptr(), len);
    _start += len;
    return len;
}

void Buffer::try_adjust()
{
    if(_start == _end)
    {
        _start = _end = 0;
        return;
    }
    /*
    if(_size >= 128 * 1024 || _start >= 0.5 * _size)
    {
        adjust();
    }
    */
}

void Buffer::adjust()
{
    memmove(_ptr, _ptr + _start, _end - _start);
    _end = _end - _start;
    _start = 0;
}

int Buffer::copy(const Buffer * src)
{
    assert(NULL != src);
    assert(NULL != src->_ptr);
    if (src->data_len() == 0)
        return 0;

    if (src->data_len() > capacity())
        return -1;

    memmove(_ptr, src->data_ptr(), src->data_len());
    _end = src->data_len();
    return data_len();
}

size_t Buffer::eat(size_t bytes)
{
    size_t len = bytes < (_end - _start) ? bytes : (_end
        - _start);
    _start += len;

    return len;
}

size_t Buffer::ignore(size_t bytes)
{
    if (bytes == 0)
        return bytes;

    if (bytes >= _end - _start) {
        size_t ret = _end - _start;

        _start = _end = 0;
        return ret;
    }

    memmove(_ptr, _ptr + _start + bytes, _end - _start
        - bytes);
    _end -= (_start + bytes);
    _start = 0;
    return bytes;
}

// .................................................................................

buffer *
buffer_init(size_t size)
{
    buffer *b;

    b = (buffer *)mcalloc(1, sizeof(*b));
    if (b) {
        b->_start = 0;
        b->_end = 0;
        b->_size = size;
        b->_max = size;
#define BUFFER_PIECE_SIZE 64
        b->_size += (BUFFER_PIECE_SIZE - 1) - ((b->_size + BUFFER_PIECE_SIZE - 1) % BUFFER_PIECE_SIZE);
        b->_max = b->_size;
#undef BUFFER_PIECE_SIZE
        b->_ptr = (char *)mcalloc(1, b->_size);
        if (b->_ptr == NULL) {
            mfree(b);
            b = NULL;
        }
    }
    return b;
}

buffer *
buffer_create_max(size_t size, size_t max)
{
    if (size > max)
        return NULL;
    buffer *b = buffer_init(size);

    if (b)
    {
        b->_max = max;
        assert((b->_size <= b->_max));
        if (b->_size > b->_max)
        {
            // comment out temp, change to log4cplus
            //ERR("buffer_create_max error! max_size=%ld, size=%ld.", b->_max, b->_size);
        }
    }
    return b;
}

void buffer_free(buffer * b)
{
    if (b) {
        mfree(b->_ptr);
        mfree(b);
    }
}

int buffer_expand(buffer * b, size_t size)
{
    assert(NULL != b);
    char *p;

    if (size >= b->_max)
    {
        //        assert(0);
        // comment out temp, change to log4cplus
        //WRN("buffer_expand error! size=%ld, max_size=%ld.", size, b->_max);
        LOG_WARN("buffer_expand error! size="<<size<<", max_size=" << b->_max);
        return BUFFER_CAPACITY_ERROR;
    }
    if (b->_size < size) {
#define BUFFER_PIECE_SIZE 64
        b->_size += (BUFFER_PIECE_SIZE - 1) - ((b->_size + BUFFER_PIECE_SIZE - 1) % BUFFER_PIECE_SIZE);
#undef BUFFER_PIECE_SIZE
        p = (char *)mrealloc(b->_ptr, size);
        if (p) {
            b->_ptr = p;
            b->_size = size;
            return 0;
        }
        else
        {
            //            assert(0);

            // comment out temp, change to log4cplus
            //WRN("buffer_expand error! b->size=%ld, max_size=%ld.", b->_size, b->_max);
            return BUFFER_MALLOC_ERROR;
        }
    }
    return 0;
}

int buffer_expand_capacity(buffer * buf, size_t cap_sz)
{
    assert(NULL != buf);
    assert(NULL != buf->_ptr);
    int ret = 0;

    if (buf->_size - buf->_end >= cap_sz)
        return 0;

    if (buffer_data_len(buf) + cap_sz >= buf->_max)
    {
        //        assert(0);
        // comment out temp, change to log4cplus
        //WRN("buffer_expand_capacity error! buffer_data_len=%ld, cap_sz=%ld buf->max=%ld.",
        //    buffer_data_len(buf), cap_sz, buf->_max);
        return BUFFER_CAPACITY_ERROR;
    }

    if (buffer_data_len(buf) + cap_sz <= buf->_size) {
        buffer_adjust(buf);
        return 0;
    }

    ret = buffer_expand(buf, buffer_data_len(buf) + cap_sz);
    if (0 != ret)
    {
        //        assert(0);
        // comment out temp, change to log4cplus
        /*WRN("buffer_expand_capacity error! buffer_data_len=%ld, cap_sz=%ld buf->max=%ld.",
            buffer_data_len(buf), cap_sz, buf->_max);*/
        return ret;
    }

    buffer_adjust(buf);
    return 0;
}

int buffer_append(buffer * dst, buffer * src)
{
    assert(NULL != dst);
    assert(NULL != src);
    assert(NULL != dst->_ptr);
    assert(NULL != src->_ptr);
    return buffer_append_ptr(dst, src->_ptr + src->_start, src->_end - src->_start);
}

int buffer_append_ptr(buffer * dst, const void *src, size_t src_len)
{
    assert(NULL != dst);
    assert(NULL != src);

    if (dst->_end - dst->_start + src_len > dst->_size)
        return BUFFER_OVER_SIZE;

    if (dst->_size - dst->_end < src_len)
        buffer_adjust(dst);

    memmove(dst->_ptr + dst->_end, src, src_len);
    dst->_end += src_len;
    return 0;
}

int buffer_append_byte(buffer * dst, unsigned char byte)
{
    return buffer_append_ptr(dst, &byte, 1);
}

int buffer_copy(buffer * dst, buffer * src)
{
    assert(NULL != dst);
    assert(NULL != src);
    assert(NULL != dst->_ptr);
    assert(NULL != src->_ptr);
    if (src->_end - src->_start == 0)
        return 0;

    if (src->_end - src->_start > dst->_size)
        return BUFFER_OVER_SIZE;

    memcpy(dst->_ptr, src->_ptr + src->_start, src->_end - src->_start);
    dst->_end = src->_end - src->_start;
    return 0;
}

size_t buffer_eat(buffer * buf, size_t bytes)
{
    assert(NULL != buf);

    size_t len = bytes < (buf->_end - buf->_start) ? bytes : (buf->_end
        - buf->_start);
    buf->_start += len;

    return len;
}

size_t buffer_ignore(buffer * buf, size_t bytes)
{
    assert(NULL != buf);

    if (bytes == 0)
        return bytes;

    if (bytes >= buf->_end - buf->_start) {
        size_t ret = buf->_end - buf->_start;

        buf->_start = buf->_end = 0;
        return ret;
    }

    memmove(buf->_ptr, buf->_ptr + buf->_start + bytes, buf->_end - buf->_start
        - bytes);
    buf->_end -= (buf->_start + bytes);
    buf->_start = 0;
    return bytes;
}

int buffer_read_fd(buffer * b, int fd)
{
    return buffer_read_fd_max(b, fd, 1024 * 1024 * 1024);
}

int buffer_read_fd_max(buffer * b, int fd, size_t max)
{
    int toread = 0, r;

    if (fd <= 0 || b == NULL)
    {
        // comment out
        LOG_ERROR("buffer_read_fd_max: BUFFER_SOCKET_ERROR: error fd or buffer null ");
        return BUFFER_SOCKET_ERROR;
    }

    if (ioctl(fd, FIONREAD, &toread)) {
        if (errno != EAGAIN && errno != EINTR) 
        {
            // comment out
            LOG_ERROR("buffer_read_fd_max: BUFFER_SOCKET_ERROR: ioctl error, errno: " <<errno);
            return BUFFER_SOCKET_ERROR;
        }
        else
        {
            return 0;
        }
    }

    /* try to detect whether it's eof */
    if (toread == 0)
        toread = 1;
    toread = toread <= (int)max ? toread : (int)max;
    if (b->_max > 0)
      toread = (toread <= ((int)b->_max - (int)buffer_data_len(b))) ? toread : ((int)b->_max - (int)buffer_data_len(b));

    if (toread >= (int)(b->_size - b->_end))
    {
        // if low on space, adjust
        buffer_adjust(b);
        if (toread >= (int)(b->_size - b->_end))
        {
            int ret = buffer_expand_capacity(b, toread);
            if (0 != ret)
            {
                // comment out
                LOG_INFO("buffer_read_fd_max: expand error! toread: " <<toread<<" ret: " <<ret);
                return ret; /* expand receive buffer */
            }
        }
    }

    toread = toread < (int)buffer_capacity(b) ? toread : (int)buffer_capacity(b);

    if (toread == 0)
    {
        // comment out
        LOG_WARN("will read 0 byte from socket!!");
        return 0;
    }
    r = read(fd, b->_ptr + b->_end, toread);
    if ((r == -1 && (errno != EINTR && errno != EAGAIN)) || r == 0)
    {
        /* connection closed or reset */
        if (r == 0)
        {
            LOG_WARN("read 0 bytes");
        }

        LOG_INFO("buffer_read_fd_max: toread: " <<toread<<" r: " <<r << " errno: "<< errno);
        return BUFFER_SOCKET_ERROR;
    }

    if (r > 0)
    {
        b->_end += r;
        return r;
    }
    else 
    {
        return 0;
    }
}

int buffer_write_fd(buffer * b, int fd)
{
    return buffer_write_fd_max(b, fd, 1024 * 1024);
}

int buffer_write_fd_max(buffer * b, int fd, size_t max)
{
    int ret = 0, total = 0;
    size_t end = b->_start + max > b->_end ? b->_end : b->_start + max;

    while (end - b->_start > 0) {
        ret = write(fd, b->_ptr + b->_start, end - b->_start);
        if (ret < 0) {
            if (EAGAIN == errno || EINTR == errno)
                break;

            return BUFFER_SOCKET_ERROR;
        }
        else if (0 == ret) {
            break;
        }
        else {
            total += ret;
            b->_start += ret;
        }
    }
    return total;
}

int
buffer_udp_recvfrom_fd(buffer *b, int fd, struct sockaddr_in &client_addr)
{
    return buffer_udp_recvfrom_fd_max(b, fd, buffer_capacity(b), client_addr);
}

int
buffer_udp_recvfrom_fd_max(buffer *b, int fd, size_t max, struct sockaddr_in &client_addr)
{
    if (fd <= 0 || b == NULL) {
        // comment out
        //ERR("error fd or buffer null");
        return BUFFER_SOCKET_ERROR;
    }
    if (max > buffer_capacity(b)) {
        return BUFFER_CAPACITY_ERROR;
    }

    socklen_t size = sizeof(struct sockaddr);
    int r = recvfrom(fd, b->_ptr + b->_end, buffer_capacity(b), 0, (struct sockaddr *)&client_addr, &size);
    if ((r == -1 && (errno != EINTR && errno != EAGAIN)) || r == 0)
    {
        /* connection closed or reset */
        if (r == 0)
        {
            //WRN("read 0 bytes");
        }
        return BUFFER_SOCKET_ERROR;
    }
    if (r > 0)
    {
        b->_end += r;
        return r;
    }
    else {
        return 0;
    }
}

int
buffer_udp_recvmsg_fd(buffer *b, int fd, struct sockaddr_in &client_addr, struct in_addr &server_addr)
{
    return buffer_udp_recvmsg_fd_max(b, fd, buffer_capacity(b), client_addr, server_addr);
}

int
buffer_udp_recvmsg_fd_max(buffer *b, int fd, size_t max, struct sockaddr_in &client_addr, struct in_addr &server_addr)
{
    if (fd <= 0 || b == NULL) {
        // comment out
        //ERR("error fd or buffer null");
        return BUFFER_SOCKET_ERROR;
    }
    if (max > buffer_capacity(b)) {
        return BUFFER_CAPACITY_ERROR;
    }

    char cmbuf[0x100];
    struct iovec iov;
    struct msghdr msg;
    msg.msg_control = cmbuf;
    msg.msg_controllen = sizeof(cmbuf);
    msg.msg_iovlen = 1;
    msg.msg_iov = &iov;
    iov.iov_len = buffer_capacity(b);
    iov.iov_base = b->_ptr + b->_end;
    msg.msg_name = &client_addr;
    msg.msg_namelen = 128;
    int r = recvmsg(fd, &msg, 0);
    if ((r == -1 && (errno != EINTR && errno != EAGAIN)) || r == 0)
    {
        /* connection closed or reset */
        if (r == 0)
        {
            //WRN("read 0 bytes");
        }
        return BUFFER_SOCKET_ERROR;
    }
    if (r > 0)
    {
        struct cmsghdr *cmsg;
        for (cmsg = CMSG_FIRSTHDR(&msg);
            cmsg != NULL;
            cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
                server_addr = ((struct in_pktinfo*)CMSG_DATA(cmsg))->ipi_addr;
            }
        }

        b->_end += r;
        return r;
    }
    else {
        return 0;
    }
}
