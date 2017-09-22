/**
 * \file
 * \brief 内存分配器, 这个分配器有啥特点？
 */
#ifndef _YUN_ALLOCATOR_H_
#define _YUN_ALLOCATOR_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


class YunAllocator
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

    size_t _freelist_index(size_t _bytes)
    {
        return (((_bytes) + (size_t)_ALIGN-1)/(size_t)_ALIGN - 1);
    }

    // Returns an object of size _n, and optionally adds to size _n free list.
    void* _refill(size_t _n);
    // Allocates a chunk for nobjs of size size.  nobjs may be reduced
    // if it is inconvenient to allocate the requested number.
    char* _chunk_alloc(size_t _size, int& _nobjs);

public:
    YunAllocator(int align = 1024, int max_bytes = 1024*1024)
            :_ALIGN(align),
            _MAX_BYTES(max_bytes),
            _NFREELISTS(0),
            _free_list(NULL),
            _base_ptr(NULL),
            _start_free(NULL),
            _end_free(NULL),
            _heap_size(0)
    {

    }

    ~YunAllocator()
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

    void* reallocate(void* _p, size_t _old_sz, size_t _new_sz);
    size_t  remain();
    size_t  heap_size(){return _heap_size;}
public:
    const int _ALIGN;
    const int _MAX_BYTES;  // 1 M
    int _NFREELISTS; // _MAX_BYTES/_ALIGN
private:
    Obj* volatile*  _free_list;
    // Chunk allocation state.
    char* _base_ptr;
    char* _start_free;
    char* _end_free;
    size_t _heap_size;

} ;




inline bool operator==(const YunAllocator&,
                       const YunAllocator&)
{
    return true;
}



#endif

