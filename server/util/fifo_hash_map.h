/**
 * @file FIFOHashmap.h
 * @brief implementation of first in first out hash map.\n
 *        Implementation elements in the order of traversal method: entrusted.\n
 *        release the memory by overloading free_memory function or foreach function.\n
 * @author renzelong
 * <pre><b>copyright: Youku</b></pre>
 * <pre><b>email: </b>renzelong@youku.com</pre>
 * <pre><b>company: </b>http://www.youku.com</pre>
 * <pre><b>All rights reserved.</b></pre>
 * @date 2014/06/30
 *
 */


#ifndef __FIFOHashmap_H__
#define __FIFOHashmap_H__

#include "type_defs.h"
#include <stdint.h>
#include <list>

#include <ext/hash_map>

template<class Key,class Value>
class CacheNodeType
{
public:
	CacheNodeType(const Key& k, const Value& v) :key(k), value(v)
	{
		
	}
			
	CacheNodeType(const CacheNodeType & other)
	{
		if(this == &other)
        {
			return;
		}		
		key = other.key;
		value = other.value;	
	}

	CacheNodeType & operator = (const CacheNodeType & other)
	{
		if(this == &other)
        {
			return *this;
		}
	
		key = other.key;
		value = other.value;
		return *this;
	}

public:
	Key key;
	Value value;
};


/**
 * @class	FIFOHashmap
 * @brief	FIFOHashmap is 
 *			add some features for FIFOHashmap, such as clear,get,set,foreach,foreach_delete.
 */
template<class Key,
class Value,
class Hash = __gnu_cxx::hash<Key>,
class Pred = std::equal_to<Key> >
class FIFOHashmap
{	
	typedef CacheNodeType<Key,Value> _node_t; 
	typedef typename std::list<_node_t> _list_t;
    typedef __gnu_cxx::hash_map<Key, typename _list_t::iterator, Hash, Pred> _map_t;
		 
public:
    typedef void(*HandlerEvent)(Value* value, void* arg);
    typedef void(*DeleteHandlerEvent)(Value* value, void* arg, uint32_t & is_destory);

	
    FIFOHashmap(const HandlerEvent & destroy_event, void* arg)
	{
        _destroy_event = destroy_event;
        _destroy_event_arg = arg;
	}
    
	virtual ~FIFOHashmap()
    {
        clear();
    }


	void set(const Key& key, const Value& value)
	{
		erase_by_key(key);
		_list.push_back(CacheNodeType<Key,Value>(key, value));
		_map[key] = --_list.end();
	}

	bool get(const Key& key, Value & value)
	{
		typename _map_t::iterator iter;
		iter = _map.find(key);
		if(iter != _map.end())
        {
			value = iter->second->value;
			return true;
		}
		return false; 
	}
	
	bool at(size_t index, Value & value)
	{
		if(_list.size() < index)
		{
			return false;
		}
		typename _list_t::iterator iter = _list.begin();
		for(size_t i = 0; i < index; i++)
		{
			if(i == index)
			{
				value = iter->value;
				return true;
			}
		}
		return false;
	}

	virtual void free_memory(Value* value)
	{
        _destroy_event(value, _destroy_event_arg);
	}

    void erase_by_key(const Key& key)
	{
		typename _map_t::iterator iter = _map.find(key);
		if(iter != _map.end())
        {
			free_memory(&iter->second->value);
			_list.erase(iter->second);
			_map.erase(iter);
		}
	}
    /*
	void erase_by_value(const Value & value)
	{
		typename _list_t::iterator iter = _list.begin();
		while(_list.end() != iter)
        {
			if(iter->value == value)
            {
				break;
			}
			iter++;
		}
		if(_list.end() == iter)
        {
			return;
		}

		Key key = iter->key;
		free_memory(&iter->value);
		_list.erase(iter);
		_map.erase(key);
	}
    */
	void clear()
	{
		while ((int)_list.size() > 0)
        {
			this->erase_by_key(_list.front().key);
		}	
	}

	size_t size()
	{
		return _list.size();
	}

	bool is_exist(const Key& key)
	{
		return (_map.find(key) != _map.end());
	}

    bool foreach(const HandlerEvent & event_pfn, void* arg)
    {
        typename _list_t::iterator iter = _list.begin();
        if (_list.end() == iter)
        {
            return false;
        }
        while (_list.end() != iter)
        {
            event_pfn(&(iter->value), arg);
            iter++;
        }
        return true;
    }

    void foreach_delete(const DeleteHandlerEvent & event_pfn, void* arg)
    {
		uint32_t is_destory = 0;
        typename _map_t::iterator iter = _map.begin();
        while (_map.end() != iter)
        {
			is_destory = 0;
            event_pfn(&iter->second->value, arg, is_destory);
            if (is_destory)
            {
				free_memory(&iter->second->value);
                _list.erase(iter->second);
                _map.erase(iter++);
            }
            else
            {
                iter++;
            }
        }
    }

	void foreach_delete_all()
	{
		typename _map_t::iterator iter = _map.begin();
        while (_map.end() != iter)
        {
            free_memory(&iter->second->value);
            _list.erase(iter->second);
            _map.erase(iter++);
        }

	}


protected:
    _list_t _list;
	_map_t	_map;
    HandlerEvent _destroy_event;
    void* _destroy_event_arg;
};

#endif /* __FIFOHashmap_H__ */

