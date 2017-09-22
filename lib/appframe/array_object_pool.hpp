/**
 * \file
 * \brief ÄÚ´æ³ØÄ£°å
 */
#ifndef _ARRAY_OBJECT_POOL_HPP_
#define _ARRAY_OBJECT_POOL_HPP_

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <new>

template<class _Tp>
class ArrayObjectPool
{
    typedef _Tp                                                         value_type;

    union Obj
    {
        Obj*       _free_list_link;
        char           _client_data[1];    /* The client sees this.        */
    };

public:
    ArrayObjectPool(int capacity)
    :_values(NULL),
    _capacity(capacity),
    _free_num(_capacity)
    {}

    ArrayObjectPool()
    :_values(NULL),
    _capacity(0),
    _free_num(0)
    {}
    
     ~ArrayObjectPool()
    {
        if(_values)
        {
            free(_values);
            _values = NULL;
        }
    }

    
    int32_t init()
    {
        assert(_capacity > 0);
        _values = (value_type*)malloc(sizeof(value_type) * _capacity);
        if(_values == NULL)
        {
            return -1;
        }
        
        _free_head._free_list_link = (Obj*)( _values);
        for(int i=0; i < _capacity - 1; i++)
        {
            ((Obj*)(_values + i))->_free_list_link =(Obj*)( _values+i+1);
        }

        ((Obj*)(_values + (_capacity - 1)))->_free_list_link = 0;

        return 0;
    }

    int32_t init(int capacity)
    {
        _capacity = capacity;
        _free_num = _capacity;
        return init();
    }
        
    int capacity(){ return _capacity;}
    
    value_type* construct()
    {
        Obj* free_node = _free_head._free_list_link;
        if(free_node == 0)
        {
            return NULL;
        }
        else
        {
            _free_head._free_list_link = free_node->_free_list_link;
            _free_num--;
            //printf("construct %p, size: %d\n",free_node, sizeof(value_type));
            return new(free_node)value_type();
        }     
    }
    
    void   destroy(value_type* node)
    {
        //printf("destroy %p, size: %d\n",node, sizeof(value_type));
        assert((node>=_values)&&(node<(_values+_capacity)));
        node->~value_type();
        ((Obj*)(node))->_free_list_link = _free_head._free_list_link;
        _free_head._free_list_link = ((Obj*)(node));
        _free_num++;
    }

    int   index(value_type* node)
    {
        return node - _values;
    }
    
    value_type& operator[](uint32_t pos)
    {
        return _values[pos];
    }

    int free_objs_num(){return _free_num;}
private:
    value_type*   _values;
    int                 _capacity;
    Obj                _free_head;
    int                 _free_num;
};

#endif

