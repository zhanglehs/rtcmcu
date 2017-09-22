/**
 * \file
 * \brief Power2 ÄÚ´æ·ÖÅäÆ÷
 */
#ifndef _POWER2_ALLOCATOR_H_
#define _POWER2_ALLOCATOR_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>


class Power2Allocator
{
private:
    union Obj
    {
        union Obj* _free_list_link;
        char           _client_data[1];    /* The client sees this.        */
    };

private:

    size_t _round_up(size_t _bytes)
    {
        return (((_bytes) + (size_t) _ALIGN-1) & ~((size_t) _ALIGN - 1));
    }

    // Returns an object of size _n, and optionally adds to size _n free list.
    void* _refill(size_t _n);
    // Allocates a chunk for nobjs of size size.  nobjs may be reduced
    // if it is inconvenient to allocate the requested number.
    char* _chunk_alloc(size_t _size, int& _nobjs);

public:
    Power2Allocator(int smallest_power = 10, int largest_power = 22)
            :_SMALLEST_POWER(smallest_power),
            _LARGEST_POWER(largest_power),
            _ALIGN(1<<_SMALLEST_POWER),
            _MAX_BYTES(1<<_LARGEST_POWER),
            _NFREELISTS(0),
            _free_list(NULL),
            _base_ptr(NULL),
            _start_free(NULL),
            _end_free(NULL),
            _heap_size(0)
    {

    }

    ~Power2Allocator()
    {
        if(_base_ptr)
        {
            delete _base_ptr;
            _base_ptr = NULL;
        }

        if(_free_list)
        {
            delete _free_list;
            _free_list = NULL;
        }
    }


    int  init(int size);

    void destroy()
    {
        free(_base_ptr);
    }

    /* _n must be > 0      */
    void* allocate(size_t _n);

    /* _p may not be 0 */
    void deallocate(void* _p, size_t _n);

    size_t  remain();
    size_t  heap_size(){return _heap_size;}
    size_t  max_bytes();
    size_t  min_bytes();
    size_t  freelist_num();
    size_t  smallest_power();
    size_t  largest_power();

    size_t  freelist_index(size_t _bytes);

    static size_t power(size_t _bytes);
public:
    const size_t _SMALLEST_POWER;
    const size_t _LARGEST_POWER;    
    
    const int _ALIGN;
    const int _MAX_BYTES;  // 1 M
    int         _NFREELISTS; // _MAX_BYTES/_ALIGN
    
private:
    Obj* volatile*  _free_list;
    // Chunk allocation state.
    char* _base_ptr;
    char* _start_free;
    char* _end_free;
    size_t _heap_size;

} ;

inline
size_t  Power2Allocator::max_bytes()
{
    return _MAX_BYTES;
}

inline
size_t  Power2Allocator::min_bytes()
{
    return _ALIGN;
}

inline
size_t  Power2Allocator::freelist_num()
{
    return _LARGEST_POWER - _SMALLEST_POWER + 1;
}

inline
size_t Power2Allocator::freelist_index(size_t _bytes)
{
    size_t index = 1;

    if(_bytes==0)
    {
        return _SMALLEST_POWER;
    }
    
    _bytes--;
    
    while(_bytes >>= 1)
    {
        index++;
    }
    
    if (index < _SMALLEST_POWER) 
    {
        index = _SMALLEST_POWER;
    }

    assert(index <= _LARGEST_POWER);
    
    return index;        
}

inline
size_t Power2Allocator::power(size_t _bytes)
{
    size_t index = 1;

    if(_bytes==0)
    {
        return 0;
    }
    
    _bytes--;
    
    while(_bytes >>= 1)
    {
        index++;
    }

    return index;
}
    
inline
size_t  Power2Allocator::smallest_power()
{
    return _SMALLEST_POWER;
}

inline
size_t  Power2Allocator::largest_power()
{
    return _LARGEST_POWER;
}
    
inline bool operator==(const Power2Allocator&,
                       const Power2Allocator&)
{
    return true;
}



#endif

