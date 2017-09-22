/**
 * \file
 * \brief 又一个内存池模板
 */
#ifndef _OBJECT_POOL_TEMPLATE_HPP_
#define _OBJECT_POOL_TEMPLATE_HPP_
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <new>


class ObjectPoolTemplate
{
    union Obj
    {
        Obj*       _free_list_link;
        char           _client_data[1];    /* The client sees this.        */
    };

    static const size_t MIN_POWER=3;
public:
	ObjectPoolTemplate()
		:_values(NULL),
		_allocated_num(0)
	{
	}
    ObjectPoolTemplate(size_t capacity, size_t max_obj_size)
    :_values(NULL),
    _capacity(capacity),
    _allocated_num(0)
    {
        _max_obj_size = round_up(max_obj_size);
    }

     ~ObjectPoolTemplate()
    {
        if(_values)
        {
            free(_values);
            _values = NULL;
        }
    }

    size_t round_up(size_t _bytes)
    {
        size_t index = 1;

        if(_bytes==0)
        {
            return MIN_POWER;
        }
        
        _bytes--;
        
        while(_bytes >>= 1)
        {
            index++;
        }
        
        if (index < MIN_POWER) 
        {
            index = MIN_POWER;
        }

        
        return 1<<index;        
    }

    int32_t init()
    {
        _values = (char*)malloc(_max_obj_size * _capacity);
        if(_values == NULL)
        {
            return -1;
        }
        
        _free_head._free_list_link = (Obj*)( _values);
        for(uint32_t i=0; i < _capacity - 1; i++)
        {
            ((Obj*)(_values + i*_max_obj_size))->_free_list_link =(Obj*)( _values+(i+1)*_max_obj_size);
        }

        ((Obj*)(_values + (_capacity - 1)*_max_obj_size))->_free_list_link = 0;

        return 0;
    }

	int32_t init(size_t capacity, size_t max_obj_size)
	{
		_capacity=capacity;
		_max_obj_size = round_up(max_obj_size);
		return init();
	}
    size_t capacity(){ return _capacity;}
	size_t size(){return _allocated_num;}
    size_t max_obj_size(){ return _max_obj_size;}

    template <class value_type>
    value_type* construct()
    {
        assert(sizeof(value_type) <= _max_obj_size);
        Obj* free_node = _free_head._free_list_link;
        if(free_node == 0)
        {
            return NULL;
        }
        else
        {
            _free_head._free_list_link = free_node->_free_list_link;
            _allocated_num++;
            return new(free_node)value_type();
        }     
    }

    template <class value_type, class param1_type>
    value_type* construct(param1_type param1)
    {
        assert(sizeof(value_type) <= _max_obj_size);
        Obj* free_node = _free_head._free_list_link;
        if(free_node == 0)
        {
            return NULL;
        }
        else
        {
            _free_head._free_list_link = free_node->_free_list_link;
            _allocated_num++;            
            return new(free_node)value_type(param1);
        }     
    }

    template <class value_type, class param1_type, class param2_type>
    value_type* construct(param1_type param1, param2_type param2)
    {
        assert(sizeof(value_type) <= _max_obj_size);
        Obj* free_node = _free_head._free_list_link;
        if(free_node == 0)
        {
            return NULL;
        }
        else
        {
            _free_head._free_list_link = free_node->_free_list_link;
            _allocated_num++;            
            return new(free_node)value_type(param1, param2);
        }     
    }
 

    template <class value_type, class param1_type, class param2_type, class param3_type>
    value_type* construct(param1_type param1, param2_type  param2, param3_type  param3)
    {
        assert(sizeof(value_type) <= _max_obj_size);
        Obj* free_node = _free_head._free_list_link;
        if(free_node == 0)
        {
            return NULL;
        }
        else
        {
            _free_head._free_list_link = free_node->_free_list_link;
            _allocated_num++;            
            return new(free_node)value_type(param1, param2, param3);
        }     
    }
    
    template <class value_type, class param1_type, class param2_type, class param3_type, class param4_type>
    value_type* construct(param1_type param1, param2_type  param2, param3_type param3, param4_type  param4)
    {
        assert(sizeof(value_type) <= _max_obj_size);
        Obj* free_node = _free_head._free_list_link;
        if(free_node == 0)
        {
            return NULL;
        }
        else
        {
            _free_head._free_list_link = free_node->_free_list_link;
            _allocated_num++;            
            return new(free_node)value_type(param1, param2, param3, param4);
        }     
    }

    template <class value_type, class param1_type, class param2_type, class param3_type, class param4_type, class param5_type>
    value_type* construct(param1_type param1, param2_type param2, param3_type param3, param4_type param4, param5_type param5)
    {
        assert(sizeof(value_type) <= _max_obj_size);
        Obj* free_node = _free_head._free_list_link;
        if(free_node == 0)
        {
            return NULL;
        }
        else
        {
            _free_head._free_list_link = free_node->_free_list_link;
            _allocated_num++;            
            return new(free_node)value_type(param1, param2, param3, param4, param5);
        }     
    }
    
    template <class value_type>    
    void   destroy(value_type* node)
    {
        node->~value_type();
        ((Obj*)(node))->_free_list_link = _free_head._free_list_link;
        _free_head._free_list_link = ((Obj*)(node));

        _allocated_num--;
    }

    template <class value_type> 
    int   index(value_type* node)
    {
        return (node - _values)/_max_obj_size;
    }    
    
private:
    char*            _values;
    size_t            _capacity;
    size_t            _max_obj_size;
    Obj               _free_head;
    int                 _allocated_num;
};

#endif

