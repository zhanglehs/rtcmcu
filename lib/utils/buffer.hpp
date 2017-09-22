/*
 * a buffer
 * author: hechao@youku.com
 * create: 2014/3/5
 *
 */

#ifndef __BUFFER_H_
#define __BUFFER_H_

#include <unistd.h>
#include <stdint.h>
#include <string.h>

namespace lcdn
{
namespace base
{

class Buffer
{
public:
    explicit Buffer(uint32_t cap);
    ~Buffer();

    // @return length of data read
    int append(const Buffer *src);
    int append(const Buffer *src, size_t src_len);
    int append_ptr(const void *src, size_t src_len);
    int append_byte(unsigned char byte);
    int read_fd(int fd);
    int read_fd_max(int fd, size_t max);


    // @return length of data writen
    int copy(const Buffer * src);
    int write_fd(int fd);
    int write_fd_max(int fd, size_t max);
    int write_buffer(Buffer *dst);
    int write_buffer(void *dst, size_t dst_len);

    void try_adjust();
    void adjust();

    size_t eat(size_t bytes);
    size_t ignore(size_t bytes);
    size_t capacity() const { return _size; }
    size_t free_size() const { return _size - _end; }
    size_t data_len() const { return _end - _start; }
    const void * data_ptr() const { return _ptr + _start; }
    void * free_ptr() { return _ptr + _end; }
    void reset() { _start = _end = 0; }

public:
    char *_ptr;
    size_t _size;
    volatile size_t _end, _start;
    size_t _max;
};

} // namespace base
} // namespace lcdn


typedef class lcdn::base::Buffer buffer;

enum BUFFER_ERROR_CODE
{
    BUFFER_OVER_SIZE = -4,
    BUFFER_CAPACITY_ERROR = -3,
    BUFFER_MALLOC_ERROR = -2,
    BUFFER_SOCKET_ERROR = -1,
};

buffer *buffer_init(size_t size);
buffer *buffer_create_max(size_t size, size_t max);
void buffer_free(buffer * buf);
int buffer_expand(buffer * buf, size_t total_sz);
int buffer_expand_capacity(buffer * buf, size_t cap_sz);
int buffer_copy(buffer * dst, buffer * src);
int buffer_append(buffer * dst, buffer * src);
int buffer_append_ptr(buffer * dst, const void *src, size_t src_len);
int buffer_append_byte(buffer * dst, unsigned char byte);
size_t buffer_eat(buffer * buf, size_t bytes);
size_t buffer_ignore(buffer * buf, size_t bytes);

// for tcp
int buffer_read_fd(buffer * buf, int fd);
int buffer_read_fd_max(buffer * buf, int fd, size_t max);
int buffer_write_fd(buffer * buf, int fd);
int buffer_write_fd_max(buffer * buf, int fd, size_t max);

// for udp
int buffer_udp_recvfrom_fd(buffer *buf, int fd, struct sockaddr_in &client_addr);
int buffer_udp_recvfrom_fd_max(buffer *buf, int fd, size_t max, struct sockaddr_in &client_addr);
int buffer_udp_recvmsg_fd(buffer *b, int fd, struct sockaddr_in &client_addr, struct in_addr &server_addr);
int buffer_udp_recvmsg_fd_max(buffer *b, int fd, size_t max, struct sockaddr_in &client_addr, struct in_addr &server_addr);

static inline size_t
buffer_size(const buffer * buf)
{
    return buf->_size;
}

static inline size_t
buffer_max(const buffer * buf)
{
    return buf->_max;
}

static inline void
buffer_reset(buffer * buf)
{
    buf->_start = buf->_end = 0;
}

static inline void
buffer_adjust(buffer * buf)
{
    memmove(buf->_ptr, buf->_ptr + buf->_start, buf->_end - buf->_start);
    buf->_end = buf->_end - buf->_start;
    buf->_start = 0;
}

static inline void
buffer_try_adjust(buffer * buf)
{
    if (buf->_start == buf->_end) {
        buf->_start = buf->_end = 0;
        return;
    }
    if (buf->_size >= 128 * 1024 || buf->_start >= buf->_size/2) {
        buffer_adjust(buf);
    }
}

//int read_dvb_buffer (int, buffer*);
static inline size_t
buffer_data_len(const buffer * buf)
{
    return buf->_end - buf->_start;
}

static inline size_t
buffer_capacity(const buffer * buf)
{
    return buf->_size - buf->_end;
}

static inline size_t
buffer_free_size(const buffer * buf)
{
    return buf->_size - (buf->_end - buf->_start);
}

static inline const char *
buffer_data_ptr(const buffer * buf)
{
    return buf->_ptr + buf->_start;
}

static inline const char *
buffer_data_end_ptr(const buffer * buf)
{
    return buf->_ptr + buf->_end;
}



#endif

