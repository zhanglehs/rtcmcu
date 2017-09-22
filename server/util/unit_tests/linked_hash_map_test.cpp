/**
* @file linked_hash_map_tests.cpp
* @brief this is unit tests for linked_hash_map
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/05/29
* @see  linked_hash_map.h
*/


#include "util/linked_hash_map.h"
#include "gtest/gtest.h"
#include "util/unit_test_defs.h"
 
struct StreamKey
{
	uint32_t stream_id;
	int seq;
	char type_id;

	StreamKey(uint32_t _stream_id, char _type_id, int _seq)
		: stream_id(_stream_id), seq(_seq), type_id(_type_id){};

	StreamKey()
		: stream_id(), seq(), type_id(){};

	bool operator==(const StreamKey& key)
	{
		if (stream_id == key.stream_id
			&& seq == key.seq
			&& type_id == key.type_id)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	bool operator!=(const StreamKey& key)
	{
		if (stream_id == key.stream_id
			&& seq == key.seq
			&& type_id == key.type_id)
		{
			return false;
		}
		else
		{
			return true;
		}
	}

};

namespace std
{
	template <>
	class equal_to<StreamKey>
	{
	public:
		bool operator()(const StreamKey& left, const StreamKey& right) const
		{
			return (left.stream_id == right.stream_id)
				&& (left.type_id == right.type_id)
				&& (left.seq == right.seq);
		}
	};

	namespace tr1
	{
		template <>
		class hash<StreamKey>
		{
		public:
			size_t operator()(const StreamKey& key) const
			{
				size_t stream_id_t = (size_t)(key.stream_id) << 17;
				size_t type_id_t = (size_t)(key.type_id) << 15;
				size_t seq_t = (size_t)(key.seq);

				size_t hash_num = stream_id_t ^ type_id_t ^ seq_t;
				return hash_num;
			}
		};
	};
}

using namespace std;
using namespace std::tr1;

typedef linked_hash_map<StreamKey, int, std::tr1::hash<StreamKey>, std::equal_to<StreamKey> > _linked_hash_map;

// get will change the sequence of linked_hash_map
TEST(linked_hash_map_test, get)
{
	_linked_hash_map* cache = new _linked_hash_map();
	cache->reset_max_size(3);
	StreamKey key;

	key.stream_id = 1;
	cache->set(key, 1);

	key.stream_id = 2;
	cache->set(key, 2);
	
	key.stream_id = 3;
	cache->set(key, 3);

	key.stream_id = 1;
	EXPECT_EQ(cache->get(key), 1);

	key.stream_id = 4;
	cache->set(key, 4);

	key.stream_id = 2;
	EXPECT_EQ(cache->get(key), 0);
}

// erase will remove an element from linked_hash_map
TEST(linked_hash_map_test, erase)
{
	_linked_hash_map* cache = new _linked_hash_map();
	cache->reset_max_size(3);
	StreamKey key;

	key.stream_id = 1;
	cache->set(key, 1);

	key.stream_id = 2;
	cache->set(key, 2);
	
	key.stream_id = 3;
	cache->set(key, 3);

	key.stream_id = 2;
	cache->erase(key);
	EXPECT_EQ(cache->size(), 2);
	EXPECT_EQ(cache->get(key), 0);

	key.stream_id = 1;
	EXPECT_EQ(cache->get(key), 1);

	key.stream_id = 3;
	EXPECT_EQ(cache->get(key), 3);
}

// reference will NOT change the sequence of linked_hash_map
TEST(linked_hash_map_test, reference)
{
	_linked_hash_map* cache = new _linked_hash_map();
	cache->reset_max_size(3);
	StreamKey key;

	key.stream_id = 1;
	cache->set(key, 1);

	key.stream_id = 2;
	cache->set(key, 2);

	key.stream_id = 3;
	cache->set(key, 3);

	key.stream_id = 1;
	EXPECT_EQ(cache->reference(key), 1);

	key.stream_id = 4;
	cache->set(key, 4);

	key.stream_id = 1;
	EXPECT_NE(cache->get(key), 1);
}

TEST(linked_hash_map_test, lock)
{
	_linked_hash_map* cache = new _linked_hash_map();
	cache->reset_max_size(3);
	StreamKey key;

	key.stream_id = 1;
	cache->set(key, 1);
	cache->lock(key);

	key.stream_id = 2; 
	cache->set(key, 2);

	key.stream_id = 3;
	cache->set(key, 3);

	key.stream_id = 4;
	cache->set(key, 4);

	key.stream_id = 1;
	EXPECT_EQ(cache->get(key), 1);

	key.stream_id = 2;
	EXPECT_NE(cache->get(key), 2);
}

TEST(linked_hash_map_test, unlock)
{
	_linked_hash_map* cache = new _linked_hash_map();
	cache->reset_max_size(3);
	StreamKey key;

	key.stream_id = 1;
	cache->set(key, 1);
	cache->lock(key);

	key.stream_id = 2;
	cache->set(key, 2);

	key.stream_id = 3;
	cache->set(key, 3);

	key.stream_id = 4;
	cache->set(key, 4);

	key.stream_id = 1;
	EXPECT_EQ(cache->get(key), 1);

	cache->unlock(key);

	key.stream_id = 2;
	cache->set(key, 2);

	key.stream_id = 3;
	cache->set(key, 3);

	key.stream_id = 4;
	cache->set(key, 4);

	key.stream_id = 1;
	EXPECT_EQ(cache->get(key), 0);
}

TEST(linked_hash_map_test, reset_max_size)
{
	_linked_hash_map* cache = new _linked_hash_map();
	cache->reset_max_size(3);
	StreamKey key;

	key.stream_id = 1;
	cache->set(key, 1);

	key.stream_id = 2;
	cache->set(key, 2);

	key.stream_id = 3;
	cache->set(key, 3);

	key.stream_id = 4;
	cache->set(key, 4);

	cache->reset_max_size(2);

	key.stream_id = 1;
	EXPECT_NE(cache->get(key), 1);

	key.stream_id = 2;
	EXPECT_NE(cache->get(key), 2);

	key.stream_id = 3;
	EXPECT_EQ(cache->get(key), 3);
}

TEST(linked_hash_map_test, reset_max_size_lock)
{
	_linked_hash_map* cache = new _linked_hash_map();
	cache->reset_max_size(4);
	StreamKey key;

	key.stream_id = 1;
	cache->set(key, 1);
	cache->lock(key);

	key.stream_id = 2;
	cache->set(key, 2);
	cache->lock(key);

	key.stream_id = 3;
	cache->set(key, 3);

	key.stream_id = 4;
	cache->set(key, 4);

	/* here the members in cache should be
		4  3  2  1
		U  U  L  L
	*/

	cache->reset_max_size(2);

	/* here the members in cache should be
	    2  1
	    L  L
	*/

	key.stream_id = 1;
	EXPECT_EQ(cache->get(key), 1);

	key.stream_id = 2;
	EXPECT_EQ(cache->get(key), 2);

	key.stream_id = 3;
	EXPECT_NE(cache->get(key), 3);

	key.stream_id = 4;
	EXPECT_NE(cache->get(key), 4);

	cache->reset_max_size(4);

	key.stream_id = 3;
	cache->set(key, 3);

	key.stream_id = 4;
	cache->set(key, 4);

	/* here the members in cache should be
	    4  3  2  1
	    U  U  L  L
	*/

	cache->reset_max_size(1);
	EXPECT_EQ(cache->size(), 1);

	/* here the members in cache should be
		2
		L
	*/

	key.stream_id = 1;
	EXPECT_NE(cache->get(key), 1);

	key.stream_id = 2;
	EXPECT_EQ(cache->get(key), 2);

	key.stream_id = 3;
	EXPECT_NE(cache->get(key), 3);

	key.stream_id = 4;
	EXPECT_NE(cache->get(key), 4);
}


