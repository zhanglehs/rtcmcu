/*
 * author: hechao@youku.com
 * create: 2014/3/10
 *
 */

#include "buffer_queue.h"

using namespace lcdn;
using namespace lcdn::base;

BufferQueue::BufferQueue()
:_buffer_size(0),
_slot_size(0),
_head(0),
_tail(0)
{
}

int BufferQueue::init(size_t buffer_size, size_t slot_size)
{
	_buffer_size = buffer_size;
	_slot_size = slot_size;
	_elements.resize(_buffer_size);
	for (int i = 0; i < _buffer_size; i++)
	{
		_elements[i] = new Buffer(_slot_size);
	}
	return buffer_size;
}

BufferQueue::BufferQueue(size_t buffer_size, size_t slot_size)
    :_buffer_size(buffer_size),
    _slot_size(slot_size),
    _head(0),
    _tail(0)
{
    _elements.reserve(_buffer_size);
    for(int i = 0; i < _buffer_size; i++)
    {
        _elements[i] = new Buffer(_slot_size);
    }

}

BufferQueue::~BufferQueue()
{
    for(int i = 0; i< _buffer_size; i++)
    {
        delete _elements[i];
        _elements[i] = NULL;
    }
}

// @return:
//      the size of data be eated;
//      -1 on error
int BufferQueue::push_data(const char * data, size_t len)
{
    if(size() == (_buffer_size -1))
    {
        return 0;
    }
    else
    {
        //size_t cap = len < _slot_size ? len : _slot_size;
        size_t cap = len < _elements[_tail]->free_size() ? len : _elements[_tail]->free_size();

        if (cap > 0)
        {
            cap = _elements[_tail]->append_ptr(data, cap);
            _tail = (_tail+1) % _buffer_size;
        }
        return cap;
    }
}

int BufferQueue::pop_data(char *dst, size_t dst_cap)
{
    if (size() == 0)
    {
        return 0;
    }

    int len = _elements[_head]->data_len();
    len = len < dst_cap ? len : dst_cap;

    //len = _elements[_head]->append_ptr(dst, len);
    len = _elements[_head]->write_buffer(dst, len);
    _elements[_head]->try_adjust();

    if (0 == _elements[_head]->data_len())
    {
        _head = (_head + 1) % _buffer_size;
    }

    return len;
}

int BufferQueue::pop_data(Buffer *dst)
{
    if (size() == 0)
    {
        return 0;
    }

    int len = _elements[_head]->write_buffer(dst);
    _elements[_head]->try_adjust();

    if (0 == _elements[_head]->data_len())
    {
        _head = (_head + 1) % _buffer_size;
    }

    return len;
}

size_t BufferQueue::size()
{
    return (_buffer_size - _head + _tail) % _buffer_size;
}

