/*
 * a queue of buffer, which supports one producer and one consumer
 * exchange data
 * 
 * author: hechao@youku.com
 * create: 2014/3/10
 *
 */

#ifndef __BUFFER_QUEUE_H_
#define __BUFFER_QUEUE_H_

#include <vector>
#include "buffer.hpp"

namespace lcdn
{
namespace base
{

class BufferQueue
{
public:
    // @buffer_size: the count of slot
    // @slot_size: size of each slot
    BufferQueue();
    int init(size_t buffer_size, size_t slot_size);
    BufferQueue(size_t buffer_size, size_t slot_size);
    ~BufferQueue();

    int push_data(const char * data, size_t size);

    // @return: the size of data be writed to dst
    // @dst: destination to which data will be write
    // @dst_cap: the capability of dst
    int pop_data(char *dst, size_t dst_cap);
    int pop_data(Buffer *dst);
    size_t size();

private:
    std::vector<Buffer*> _elements;
    size_t _buffer_size;    // count of _elements
    size_t _slot_size; // size of each slot
    size_t _head;
    size_t _tail;

};

} // namespace lcdn
} // namespace base

#endif

