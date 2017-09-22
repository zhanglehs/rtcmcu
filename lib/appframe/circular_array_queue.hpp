/**
 * \file
 * \brief LockFree ¶ÓÁÐÄ£°å
 */
#ifndef _CIRCULAR_ARRAY_QUEUE_
#define  _CIRCULAR_ARRAY_QUEUE_

#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>

template <typename ElementType>
class CircularArrayQueue
{
public:
    typedef ElementType               value_type;
    typedef ElementType&                 reference;
    typedef const ElementType&          const_reference;

public:
    /**
     *  @brief  Default constructor creates no elements.
     */
    explicit
    CircularArrayQueue(size_t  capacity )
            :_elements(NULL),
            _head(0),
            _tail(0),
            _CAPACITY(capacity)
    {
        _elements = new value_type[_CAPACITY];
    }

    CircularArrayQueue( )
    {
        _elements = NULL;
        _head = 0;
        _tail = 0;
    }

    ~CircularArrayQueue()
    {
        delete[]  _elements;
    }

    int init(size_t  capacity)
    {
        if(NULL != _elements)
        {
            return -1;
        }
        
        _elements = new value_type[capacity];
        if(NULL == _elements)
        {
            return -1;
        }
        _CAPACITY = capacity;
        return 0;
    }
    /**
     *  Returns true if the %queue is empty.
     */
    size_t capacity()
    {
        return _CAPACITY;
    }

    bool full()
    {
        return size() == (_CAPACITY -1);
    }
    bool
    empty() const
    {
        return _head == _tail;
    }

    /**  Returns the number of elements in the %queue.  */
    size_t
    size() const
    {
        return (_CAPACITY -_head+_tail) %_CAPACITY;
    }

    /**  Returns the number of elements in the %queue.  */
    size_t
    free() const
    {
        return _CAPACITY - (_CAPACITY -_head+_tail) %_CAPACITY;
    }

    /**
     *  Returns a read/write reference to the data at the first
     *  element of the %queue.
     */
    reference
    front()
    {
        return _elements[_head];
    }

    /**
     *  Returns a read-only (constant) reference to the data at the first
     *  element of the %queue.
     */
    const_reference
    front() const
    {
        return _elements[_head];

    }

    /**
     *  Returns a read/write reference to the data at the last
     *  element of the %queue.
     */
    reference
    back()
    {
        return _elements[(_CAPACITY +_tail -1) % _CAPACITY];
    }

    /**
     *  Returns a read-only (constant) reference to the data at the last
     *  element of the %queue.
     */
    const_reference
    back() const
    {
        return _elements[(_CAPACITY +_tail -1) % _CAPACITY];
    }

    /**
     *  @brief  Add data to the end of the %queue.
     *  @param  x  Data to be added.
     *
     *  This is a typical %queue operation.  The function creates an
     *  element at the end of the %queue and assigns the given data
     *  to it.  The time complexity of the operation depends on the
     *  underlying sequence.
     */
    int
    push(const value_type& value)
    {
        if(size() == (_CAPACITY -1))
        {
            return -1;
        }
        else
        {
            _elements[_tail] = value;
            _tail = (_tail+1) % _CAPACITY;
            return 0;
        }
    }

    void
    pop()
    {
        if(empty())
        {
            return;
        }
        else
        {
            _head = (_head + 1) % _CAPACITY;
        }
    }
    inline
    void pop(reference r)
    {
        if(empty())
            return;
        r=front();
        pop();
        return;
    }

    void clear()
    {
        _head = 0;
        _tail = 0;
    }

private:

    value_type* _elements;
    volatile  uint32_t      _head;
    volatile uint32_t      _tail;
    size_t            _CAPACITY;

};

#endif
