#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "power2_allocator.h"



int  Power2Allocator::init(int size)
{
    if((_MAX_BYTES <=0) || (_ALIGN <=0 ))
    {
        return -1;
    }

    _NFREELISTS = _LARGEST_POWER+1;

    _free_list = new Obj* volatile[_NFREELISTS];

    for(int i=0; i < _NFREELISTS; i++)
    {
        _free_list[i] = 0;
    }

    int real_size = size;//_round_up(size);
    _start_free = (char*)malloc(real_size);
    if(_start_free == NULL)
    {
        return -1;
    }
    else
    {
        _base_ptr       =  _start_free;
        _end_free       =  _start_free + real_size;
        _heap_size     = real_size;

        //为防止某个桶饿死的情况，预先为每个桶分配10个空间快
        for(uint32_t i=_SMALLEST_POWER; i <= _LARGEST_POWER; i++)
        {
            for(int32_t j=0; j<(1<<(_LARGEST_POWER-i)); j++)
            {
                void* p = allocate(1<<i);
                if(p == NULL)
                {
                    return -1;
                }
                deallocate(p, 1<<i);
            }
        }
        
        return _heap_size;
    }
}

// The 16 zeros are necessary to make version 4.1 of the SunPro
// compiler happy.  Otherwise it appears to allocate too little
// space for the array.
/* We allocate memory in large chunks in order to avoid fragmenting     */
/* the malloc heap too much.                                            */
/* We assume that size is properly aligned.                             */
/* We hold the allocation lock.                                         */

char* Power2Allocator::_chunk_alloc(size_t _size,
                                  int& _nobjs)
{
    char* _result;
    size_t _total_bytes = _size * _nobjs;
    size_t _bytes_left = _end_free - _start_free;

    if (_bytes_left >= _total_bytes)
    {
        _result = _start_free;
        _start_free += _total_bytes;
        return(_result);
    }
    else if (_bytes_left >= _size)
    {
        _nobjs = (int)(_bytes_left/_size);
        _total_bytes = _size * _nobjs;
        _result = _start_free;
        _start_free += _total_bytes;
        return(_result);
    }
    else
    {
        /*
        size_t _bytes_to_get =
            2 * _total_bytes + _round_up(_heap_size >> 4);
        */
        // Try to make use of the left-over piece.
        /*
        if (_bytes_left > 0)
        {
            Obj* volatile* _my_free_list =
                _free_list + freelist_index(_bytes_left);

            ((Obj*)_start_free) -> _free_list_link = *_my_free_list;
            *_my_free_list = (Obj*)_start_free;
        }
        */
        return NULL;
        /* _start_free = (char*)malloc(_bytes_to_get);

        if (0 == _start_free)
        {
           size_t _i;
           Obj* volatile* _my_free_list;
           Obj* _p;
           // Try to make do with what we have.  That can't
           // hurt.  We do not try smaller requests, since that tends
           // to result in disaster on multi-process machines.
           for (_i = _size;
                   _i <= (size_t) _MAX_BYTES;
                   _i += (size_t) _ALIGN)
           {
               _my_free_list = _free_list + freelist_index(_i);
               _p = *_my_free_list;
               if (0 != _p)
               {
                   *_my_free_list = _p -> _free_list_link;
                   _start_free = (char*)_p;
                   _end_free = _start_free + _i;
                   return(_chunk_alloc(_size, _nobjs));
                   // Any leftover piece will eventually make it to the
                   // right free list.
               }
           }
           _end_free = 0;	// In case of exception.
           _start_free = (char*)malloc_alloc::allocate(_bytes_to_get);
           // This should either throw an
           // exception or remedy the situation.  Thus we assume it
           // succeeded.
        }

        _heap_size += _bytes_to_get;
        _end_free = _start_free + _bytes_to_get;
        return(_chunk_alloc(_size, _nobjs));
        */
    }
}


/* Returns an object of size _n, and optionally adds to size _n free list.*/
/* We assume that _n is properly aligned.                                */
/* We hold the allocation lock.                                         */

void* Power2Allocator::_refill(size_t _n)
{
    int _nobjs = 10;
    char* _chunk = _chunk_alloc(_n, _nobjs);
    Obj* volatile* _my_free_list;
    Obj* _result;
    Obj* _current_obj;
    Obj* _next_obj;
    int _i;

    if(_chunk == NULL)
    {
        return NULL;
    }

    if (1 == _nobjs) return(_chunk);
    _my_free_list = _free_list + freelist_index(_n);

    /* Build free list in chunk */
    _result = (Obj*)_chunk;
    *_my_free_list = _next_obj = (Obj*)(_chunk + _n);
    for (_i = 1; ; _i++)
    {
        _current_obj = _next_obj;
        _next_obj = (Obj*)((char*)_next_obj + _n);

        if (_nobjs - 1 == _i)
        {
            _current_obj -> _free_list_link = 0;
            break;
        }
        else
        {
            _current_obj -> _free_list_link = _next_obj;
        }
    }
    return(_result);
}


/* _n must be > 0      */
void* Power2Allocator::allocate(size_t _n)
{
    void* ret = 0;

    if (_n > (size_t) _MAX_BYTES)
    {
        return NULL;
    }
    else
    {
        size_t list_index = freelist_index(_n);
        
        Obj* volatile* _my_free_list = _free_list + list_index;

        Obj*  _result = *_my_free_list;
        if (_result == 0)
        {
            size_t to_fill = 1<<list_index;
            ret = _refill(to_fill); 
        }
        else
        {
            *_my_free_list = _result -> _free_list_link;
            ret = _result;
        }
    }

    return ret;
};

/* _p may not be 0 */
void Power2Allocator::deallocate(void* _p, size_t _n)
{
    if (_n > (size_t) _MAX_BYTES)
    {
        assert(false);
        return;
    }
    else
    {
        Obj* volatile*  _my_free_list = _free_list + freelist_index(_n);
        Obj* _q = (Obj*)_p;

        _q -> _free_list_link = *_my_free_list;
        *_my_free_list = _q;
    }
}


size_t  Power2Allocator::remain()
{
    size_t total = 0;
    
    for(uint32_t i=_SMALLEST_POWER; i <= _LARGEST_POWER; i++)
    {
        Obj* volatile  _my_free_list = _free_list[i];
        while(_my_free_list != 0)
        {
            total+= 1<<i;
            _my_free_list = _my_free_list -> _free_list_link;
        }
    }
    return total;
}


