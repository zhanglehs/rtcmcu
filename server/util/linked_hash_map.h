/**
 * @file linked_hash_map.h
 * @brief implementation of java's interface LinkedHashMap<K,V> using C++.
 * @author songshenyi
 * <pre><b>copyright: Youku</b></pre>
 * <pre><b>email: </b>songshenyi@youku.com</pre>
 * <pre><b>company: </b>http://www.youku.com</pre>
 * <pre><b>All rights reserved.</b></pre>
 * @date 2014/05/22
 * @see http://docs.oracle.com/javase/7/docs/api/java/util/LinkedHashMap.html
 *
 */


#ifndef __LINKED_HASH_MAP_H__
#define __LINKED_HASH_MAP_H__

#include <common/type_defs.h>
#include <limits.h>
#include <list>

#if __GNUC__ > 3
#include <tr1/unordered_map>
#else
#include <ext/hash_map>
#endif

/**
 * @class	linked_hash_map
 * @brief	linked_hash_map is 
 *			add some features for linked_hash_map, such as lock, unlock, reference.
 */
template<class Key,
class Value,
#if __GNUC__ > 3
class Hash = std::tr1::hash<Key>,
#else
class Hash = __gnu_cxx::hash<Key>,
#endif
class Pred = ::std::equal_to<Key> >
class linked_hash_map
{
public: 
	class CacheNode;

#if __GNUC__ > 3
	typedef std::tr1::unordered_map<Key, typename std::list<CacheNode>::iterator, Hash, Pred> Hash_Map;
#else
    typedef __gnu_cxx::hash_map<Key, typename ::std::list<CacheNode>::iterator, Hash, Pred> Hash_Map;
#endif

public:
	/**
	 * @fn		linked_hash_map::linked_hash_map() 
	 * @brief	the initial _max_size will be set to INT_MAX.
	 *			the initial _access_order_by_get is true, it means it is an LRU cache.
	 */
	linked_hash_map()
	{
		_max_size = INT_MAX;
		_access_order_by_get = true;
		memset(&_null_v, 0, sizeof(_null_v));
	}

	/**
	 * @fn		linked_hash_map::linked_hash_map(int max_size, bool access_order_by_get = true)
	 * @brief	you can specify the max_size and access_order_by_get. \n
	 * @param[in] max_size max size of linked_hash_map.
	 * @param[in] access_order_by_get whether change order when get.
	 * @see		_access_order_by_get
	 */
	linked_hash_map(int max_size, bool access_order_by_get = true)
	{
		reset_max_size(max_size);
		_access_order_by_get = access_order_by_get;
		memset(&_null_v, 0, sizeof(_null_v));
	}

	/**
	 * @fn		int linked_hash_map::reset_max_size(int max_size)
	 * @brief	reset the max size of linked_hash_map, if the input number is bigger \n
	 *			than size, will remove the tail items.
	 * @param[in] max_size the new max_size
	 * @return	the new max_size. 
	 */
	int reset_max_size(int max_size)
	{
		if (max_size < 1)
		{
			max_size = 1;
		}

		_max_size = max_size;

		_cut_head();

		return _max_size;
	}

	/**
	 * @fn		Key& linked_hash_map::set(const Key& key, const Value& value)
	 * @brief	set a new value for a key. it will replace the key if it exists.
	 *			this is a shallow copy.
	 * @param[in] key	the reference of key.
	 * @param[in] value the reference of value.
	 * @return	the reference of key.
	 */
	Key& set(const Key& key, const Value& value)
	{
		typename Hash_Map::iterator it;

		if (try_get(key, it))
		{
			free_memory(key);
			it->second->value = value;
			_adjust(key, true);
		}
		else
		{
			// insert a new key into map
			// 1. insert node to back (back)
			_list.push_back(CacheNode(key, value));
			_hash_map[key] = -- _list.end();

			// 2. if no space, remove tail node (back)
			_cut_head();
		}

		return (Key&)key;
	}

	/**
	* @fn		bool try_get(const Key& key, typename Hash_Map::iterator& it)
	* @brief	try to get value iterator of a key.
	* @param[in] key the reference of key.
	* @param[out] it the iterator of value.
	* @return	if _hash_map contains this value.
	*/
	bool try_get(const Key& key, typename Hash_Map::iterator& it)
	{
		it = _hash_map.find(key);
		return (it != _hash_map.end());
	}

	/**
	 * @fn		Value& linked_hash_map::get(const Key& key)
	 * @brief	get value of a key. this operation will change the order of linked hash map.
	 * @param[in] key the reference of key.
	 * @return	the reference of the value, if the key not exist, it will return NULL.
	 */
	Value& get(const Key& key)
	{
		typename Hash_Map::iterator it;

		if (try_get(key, it))
		{
			_adjust(key, _access_order_by_get);

			return it->second->value;
		}
		else
		{
			return _null_v; 
		}
	}

	/**
	 * @fn		Value& linked_hash_map::reference(const Key& key)
	 * @brief	if you want to get a value an not change anything of linked hash map, you can use reference.
	 * @param[in] key the reference of key.
	 * @return	the reference of the value, if the key not exist, it will return NULL.
	 */
	Value& reference(const Key& key)
	{
		typename Hash_Map::iterator it;

		if (try_get(key, it))
		{
			return it->second->value;
		}
		else
		{
			return _null_v;
		}
	}

	/**
	 * @fn		bool linked_hash_map::lock(const Key& key)
	 * @brief	if a key was locked, it will not be erased in first round _cut_tail().
	 * @param[in] key the reference of key.
	 * @return	if this key was locked, return true, else false.
	 */
	bool lock(const Key& key)
	{
		typename Hash_Map::iterator it;

		if (try_get(key, it))
		{
			it->second->locked = true;
			return true;
		}
		else
		{
			return false;
		}
	}

	/**
	 * @fn		bool linked_hash_map::unlock(const Key& key)
	 * @brief	unlock a locked item..
	 * @param[in] key the reference of key.
	 * @return	if this key was unlocked, return true, else false.
	 */
	bool unlock(const Key& key)
	{
		typename Hash_Map::iterator it;

		if (try_get(key, it))
		{
			it->second->locked = false;
			return true;
		}
		else
		{
			return false;
		}
	}

	/**
	 * @fn		int linked_hash_map::free_memory(const Key& key)
	 * @brief	if you want to do something before erase an item, you can override this.
	 * @param[in] key the reference of key.
	 * @return	0
	 */
	virtual int free_memory(const Key& key)
	{
		return 0;
	}

	/**
	 * @fn				int linked_hash_map::erase(const Key& key)
	 * @brief			Erase an element. 
	 * @param[in] key	The key and value you want to erase.
	 * @return			size of linked_hash_map, if this key not exist, return -1;
	 */
	virtual int erase(const Key& key)
	{
		typename Hash_Map::iterator it;

		if (try_get(key, it))
		{
			free_memory(key);
			typename std::list<CacheNode>::iterator list_it = it->second;
			_list.erase(list_it);
			_hash_map.erase(it);
			return size();
		}
		else 
		{
			return -1;
		}
	}

	/**
	 * @fn		int linked_hash_map::size()
	 * @brief	size of linked hash map.
	 * @return  size.
	 */
	int size()
	{
		return _list.size();
	}

	/**
	 * @fn		bool linked_hash_map::contains(const Key& key)
	 * @brief	whether contains this key.
	 * @param[in] key the key you want to check.
	 * @return	true or false.
	 */
	bool contains(const Key& key)
	{
		return (_hash_map.find(key) != _hash_map.end());
	}

	/**
	 * @fn		Value& linked_hash_map::oldest()
	 * @brief	the oldest accessed item.
	 * @return	the oldest accessed item.
	 */
	Value& oldest()
	{
		if (size() != 0)
		{
			return _list.front();
		}
		else
		{
			return _null_v;
		}
	}

	/**
	 * @fn		Value& linked_hash_map::latest()
	 * @brief	the latest accessed item.
	 * @return	the latest accessed item.
	 */
	Value& latest()
	{
		if (size() != 0)
		{
			return _list.back();
		}
		else
		{
			return _null_v;
		}
	}

	/**
	 * @fn		linked_hash_map::~linked_hash_map()
	 * @brief	destructor.
	 */
	virtual ~linked_hash_map()
	{
		_max_size = 0;

		while ((int)_list.size() > _max_size)
		{
			this->erase(_list.front().key);
		}
	}

public:

	/**
	 * @class	CacheNode
	 * @brief	the cache node of linked hash map. \n
	 */
	class CacheNode
	{
	public:

		/**
		 * @fn		CacheNode::CacheNode(const Key& k, const Value& v)
		 * @brief	constructor
		 * @param[in] k key
		 * @param[in] v value
		 */
		CacheNode(const Key& k, const Value& v) :key(k), value(v), locked(false)
		{}


		/**
		* @fn		CacheNode::CacheNode(const CacheNode& right)
		* @brief	copy constructor
		* @param[in] right source CacheNode
		*/
		CacheNode(const CacheNode& right)
			: key(right.key), value(right.value), locked(right.locked)
		{
		}
	public:

		/**
		 * Key
		 */
		Key key;

		/**
		 * Value
		 */
		Value value;

		/**
		 * lock flag
		 */
		bool locked;
	};

protected:

	/**
	 * @fn		linked_hash_map::Key& _adjust(const Key& key, bool change_seq)
	 * @brief	change the order, move one item to the begin of list.
	 * @param[in] key key
	 * @param[in] change_seq whether change the sequence.
	 * @return	this key.
	 */
	virtual Key& _adjust(const Key& key, bool change_seq)
	{
		if (change_seq)
		{
			_list.splice(_list.end(), _list, _hash_map[key]);
			_hash_map[key] = -- _list.end();
		}

		return (Key&)key;
	}

	/**
	 * @fn		int linked_hash_map::_cut_tail()
	 * @brief	cut the tail items. it will cut 25% tail items.
	 * @return	size after cut.
	 */
	virtual int _cut_head()
	{
		if ((int)_list.size() <= _max_size)
		{
			return _list.size();
		}

		int remain_size = (int)(_max_size - (_max_size>>2));
		if (_max_size < 4)
		{
			remain_size = _max_size;
		}

		Key& back_key = _list.back().key;

		// if a node was locked, adjust it to front.
		while ((int)_list.size() > remain_size && back_key != _list.front().key)
		{
			if (_list.front().locked)
			{
				_adjust(_list.front().key, true);
			}
			else
			{
				this->erase(_list.front().key);
			}
		}

		// if all nodes were locked, erase back nodes.
		while ((int)_list.size() > remain_size)
		{
			this->erase(_list.front().key);
		}

		return _list.size();
	}

protected:

	/**
	 * max size of linked hash map.
	 */
	int			_max_size;

	/**
	 * If access_order_by_get == true, it means LRU (latest recently used),
	 * when an item was got, it will splice to the header of the store.
	 * The item which oldest got will be dropped. \n
	 * If access_order_by_get == false, it means FIFO (first in first out),
	 * the oldest set item will be dropped.
	 */
	bool		_access_order_by_get;

	/**
	 * store the cache node
	 */
	std::list<CacheNode>		_list;

	/**
	 * key-value hashmap
	 */
#if __GNUC__ > 3
	std::tr1::unordered_map<Key, typename std::list<CacheNode>::iterator, Hash, Pred>	_hash_map;
#else
	__gnu_cxx::hash_map<Key, typename std::list<CacheNode>::iterator, __gnu_cxx::hash<Key>, Pred>		_hash_map;
#endif

	/**
	 * null value.
	 */
	Value		_null_v;
};

#endif /* __LINKED_HASH_MAP_H__ */
