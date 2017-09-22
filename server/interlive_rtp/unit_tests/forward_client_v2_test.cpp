
/**
* @file forward_client_v2_test.cpp
* @brief this is unit tests for forward_client_v2
* @author renzelong
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>renzelong@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/07/14
* @see  forward_client_v2.h \n
*		forward_client_v2.cpp
*/

#define protected public
#define private public

#include <string.h>

#include <iostream>
#include <fstream>
#include "gtest/gtest.h"
#include "util/unit_test_defs.h"
#include "../forward_client_v2.h"
#include "../module_backend.h"

using namespace std;
using namespace forward_client;


using namespace media_manager;


class PlayerCacheManagerImp : public PlayerCacheManagerInterface
{

	public:
		fragment::FLVBlock* get_latest_block(uint32_t stream_id, uint8_t rate_type,\
				int32_t& status_code, bool only_check)
		{
			return NULL;
		}

		fragment::FLVBlock* get_latest_key_block(uint32_t stream_id, uint8_t rate_type,\
				int32_t& status_code, bool only_check)
		{
			return NULL;
		}
		fragment::FLVBlock* get_block_by_global_seq(uint32_t stream_id, uint8_t rate_type, \
				int32_t seq, int32_t& status_code, bool return_next_if_not_found, bool only_check)
		{
			return NULL;
		}
		fragment::FLVBlock* get_next_block_by_global_seq(uint32_t stream_id, uint8_t rate_type,\
				int32_t seq, int32_t& status_code, bool only_check)
		{
			return NULL;
		}

		bool contains_stream(uint32_t, int32_t&)
		{
			return true;
		}
		bool contains_ratetype(uint32_t, uint8_t, int32_t&)
		{
			return true;
		}
		int32_t get_flv_header(uint32_t, uint8_t, buffer*, int32_t&)
		{
			return 0;
		}

		flv_header* get_flv_header(uint32_t, uint8_t, int32_t&, int32_t&, bool)
		{
			return NULL;
		}

			
		fragment::FLVFragment* get_fragment_by_seq(uint32_t stream_id, uint8_t rate_type, int32_t fragment_seq, int32_t& status_code, bool only_check)
		{
			return 0;
		}
		int32_t register_watcher(cache_watch_handler handler, uint8_t watch_type = CACHE_WATCHING_ALL)
		{
			return 0;
		}

};

PlayerCacheManagerInterface* CacheManager::get_player_cache_instance()
{
	return new PlayerCacheManagerImp();
}

class SmartMemoryPoolTest : public testing::Test
{
	protected:
		virtual void SetUp()
		{
		}

	virtual void TearDown()
	{
	}

public:
	SmartMemoryPool<MemoryItem> _mp;
};

TEST_F(SmartMemoryPoolTest, test_success_create)
{
	ASSERT_EQ(ERR_CODE_OK == _mp.create(10, 128, 10UL) && \
		10 == _mp._available_items.size() && \
		10 == _mp._recy_time_len, true);
}

TEST_F(SmartMemoryPoolTest, test_success_destory)
{
	_mp.destory();
	ASSERT_EQ(0 == _mp._available_items.size(), true);
}

TEST_F(SmartMemoryPoolTest, test_failed_create)
{
	_mp.destory();
	ASSERT_EQ(ERR_CODE_OK == _mp.create(20000 * 50, 1024 * 1024 * 1024, 10), false);
	_mp.destory();
}

TEST_F(SmartMemoryPoolTest, test_success_malloc)
{
	_mp.destory();
	_mp.create(10, 128, 10);
	ASSERT_EQ(NULL != _mp.malloc() && _mp._available_items.size() == 9 && _mp._occupied_items.size() == 1, true);
	_mp.destory();
}

TEST_F(SmartMemoryPoolTest, test_failed_malloc)
{
	_mp.destory();
	ASSERT_EQ(NULL == _mp.malloc(), true);
}

TEST_F(SmartMemoryPoolTest, test_success_remove_item)
{
	_mp.create(10, 128, 10);
	MemoryBlock<MemoryItem>* block = _mp.malloc();
	_mp._remove_item(_mp._occupied_items, MBSTATE_OCCUPIED);
	ASSERT_EQ(_mp._occupied_items.size() == 0, true);
	_mp.free(&block);
	_mp.destory();
}

TEST_F(SmartMemoryPoolTest, test_success_remove_item_obj)
{
	_mp.create(10, 128, 10);
	MemoryBlock<MemoryItem>* block = _mp.malloc();
	_mp._remove_item(_mp._occupied_items, block);
	ASSERT_EQ(_mp._occupied_items.size() == 0, true);
	_mp.free(&block);
	_mp.destory();
}

TEST_F(SmartMemoryPoolTest, test_success_free)
{
	MemoryBlock<MemoryItem>* block = NULL;
	_mp.create(10, 128, 10);
	block = _mp.malloc();
	_mp.free(&block);
	ASSERT_EQ(NULL == block, true);
	_mp.destory();
}

TEST_F(SmartMemoryPoolTest, test_failed_free)
{
	MemoryBlock<MemoryItem>* block = NULL;
	_mp.free(&block);
}


TEST_F(SmartMemoryPoolTest, test_success_recycling)
{
	_mp.create(10, 128, 10);
	_mp.malloc()->state = MBSTATE_FREE;
	_mp.malloc()->state = MBSTATE_FREE;
	_mp._recycling();
	ASSERT_EQ(_mp._occupied_items.size() == 0, true);
	_mp.destory();
}

TEST_F(SmartMemoryPoolTest, test_success_timer)
{
	_mp.create(10, 128, 10);
	_mp.malloc()->state = MBSTATE_FREE;
	_mp.malloc()->state = MBSTATE_FREE;
	_mp._recy_time_last = time(NULL) - 11;
	_mp.timer();
	ASSERT_EQ(_mp._occupied_items.size() == 0, true);
	_mp.destory();
}

class CacheInfoMgrTest : public testing::Test
{
protected:

	virtual void SetUp()
	{

	}

	virtual void TearDown()
	{

	}

public:
	CacheInfoMgr _cache;
};

TEST_F(CacheInfoMgrTest, test_success_create)
{
	uint32_t max_size = 10;
	ASSERT_EQ(_cache.create(max_size) && _cache._caches.size() == 0 && _cache._memory_pool.get_available_size() == max_size, true);
}

TEST_F(CacheInfoMgrTest, test_failed_create)
{
	uint32_t max_size = 0;
	_cache.destory();
	ASSERT_EQ(!_cache.create(max_size) && _cache._caches.size() == 0 && _cache._memory_pool.get_available_size() == 0, true);
}

TEST_F(CacheInfoMgrTest, test_failed_create_re)
{
	uint32_t max_size = 10;

	ASSERT_EQ(_cache.create(max_size) && _cache._caches.size() == 0 && _cache._memory_pool.get_available_size() == max_size, true);
	ASSERT_EQ(!_cache.create(max_size) && _cache._caches.size() == 0, true);
}

TEST_F(CacheInfoMgrTest, test_success_destory)
{
	_cache.destory();
	ASSERT_EQ(!_cache._is_created &&  _cache._caches.size() == 0 && _cache._memory_pool.get_available_size() == 0, true);
}

TEST_F(CacheInfoMgrTest, test_success_add)
{
	cache_info_key_t key = 1;
	SmartMemoryPool<MemoryItem> smp;
	uint32_t max_size = 10;
	smp.create(10, 128, 10);
	MemoryBlock<MemoryItem>* block = smp.malloc();

	_cache.destory();
	_cache.create(max_size);
	ASSERT_EQ(_cache.add(key, block) && _cache.get_count(key) == 1, true);
	smp.destory();
	_cache.destory();
}

TEST_F(CacheInfoMgrTest, test_failed_add)
{
	cache_info_key_t key = 2;
	MemoryBlock<MemoryItem>* block = NULL;
	uint32_t max_size = 10;
	_cache.create(max_size);
	ASSERT_EQ(!_cache.add(key, block) && _cache.get_count(key) == 0, true);
	_cache.destory();

}

TEST_F(CacheInfoMgrTest, test_success_get)
{

	cache_info_key_t key = 3;
	uint32_t max_size = 10;
	task_id_t task_id = 1024;
	SmartMemoryPool<MemoryItem> smp;
	smp.create(max_size, 128, 10);
	MemoryBlock<MemoryItem>* block = smp.malloc();
	block->item.task_id = task_id;
	_cache.destory();
	_cache.create(max_size);
	_cache.add(key, block);
	block = NULL;

	ASSERT_EQ(_cache.get(key, (void**)&block) && block != NULL && block->item.task_id == task_id && \
		_cache.get_count(key) == 0 && _cache._memory_pool.get_available_size() == max_size, true);

	_cache.destory();
}

TEST_F(CacheInfoMgrTest, test_success_get_obj)
{
	cache_info_key_t key = 4;
	uint32_t max_size = 2;
	task_id_t task_id = 1024;
	SmartMemoryPool<MemoryItem> smp;
	smp.create(max_size, 128, 10);
	MemoryBlock<MemoryItem>* block = smp.malloc();
	block->item.task_id = task_id;

	_cache.destory();
	_cache.create(max_size);
	_cache.add(key, block);
	block = NULL;

	ASSERT_EQ((block = (MemoryBlock<MemoryItem>*)_cache.get(key)) != NULL && block->item.task_id == task_id && \
		_cache.get_count(key) == 0 && _cache._memory_pool.get_available_size() == max_size, true);
}

TEST_F(CacheInfoMgrTest, test_failed_get)
{
	cache_info_key_t key = 6;
	uint32_t max_size = 10;


	_cache.destory();
	_cache.create(max_size);

	void* block = NULL;
	ASSERT_EQ(!_cache.get(key, &block) && block == NULL, true);
}

TEST_F(CacheInfoMgrTest, test_failed_get_obj)
{
	cache_info_key_t key = 5;
	uint32_t max_size = 2;

	_cache.destory();
	_cache.create(max_size);

	void* block = NULL;
	ASSERT_EQ((block = _cache.get(key)) == NULL, true);
}

TEST_F(CacheInfoMgrTest, test_success_remove_timeout)
{
	cache_info_key_t key = 1;
	SmartMemoryPool<MemoryItem> smp;
	uint32_t max_size = 10;
	smp.create(10, 128, 10);
	MemoryBlock<MemoryItem>* block = smp.malloc();

	_cache.destory();
	_cache.create(max_size);
	_cache.add(key, block);
	_cache.add(key, smp.malloc());
	_cache.add(key, smp.malloc());
	_cache.add(key, smp.malloc());
	_cache.add(key, smp.malloc());

	sleep(2);
	std::set<void*> sets;
	_cache.remove_timeout(1, sets);
	ASSERT_EQ(sets.size() == 5 && _cache.get_count(key) == 0 && \
		_cache._memory_pool.get_available_size() == max_size && \
		_cache._caches.size() == 0, true);
	smp.destory();
	_cache.destory();
}

class HistoryRequestTest : public testing::Test
{
protected:

	virtual void SetUp()
	{

	}

	virtual void TearDown()
	{

	}

public:
	HistoryRequest _request;
};

TEST_F(HistoryRequestTest, insert)
{
	what_t whatx;
	string str = "xiaomao,tian tian test!";
    whatx.generate_debug_header(str);
	ASSERT_EQ(_request.insert(1, whatx, 1), true);
	ASSERT_EQ(_request._task_list.size() == 1, true);
}

TEST_F(HistoryRequestTest, insert_failed)
{
	what_t whatx;
	string str = "xiaomao,tian tian test!";
    whatx.generate_debug_header(str);
	ASSERT_EQ(_request.insert(1, whatx, 2), true);
	ASSERT_EQ(_request._task_list.size() == 1, true);
}

TEST_F(HistoryRequestTest, delete_by_task_id)
{
	cache_request_t whatx;
	string str = "laifeng,tian tian test!";
    whatx.generate_debug_header(str);
	_request.insert(1, whatx, 3);
	_request.insert(2, whatx, 4);
	_request.delete_by_task_id(3);
	task_id_t task_id = _request.get_task_id(1, whatx);
	ASSERT_EQ(task_id == 0, true);
}

TEST_F(HistoryRequestTest, delete_by_task_id_failed)
{
	cache_request_t whatx;
	string str = "laifeng,tian tian test!";
    whatx.generate_debug_header(str);
	_request.clear();
	_request.insert(1, whatx, 3);

	str = "343434,tian tian test2!";
	whatx.generate_debug_header(str);
	_request.insert(2, whatx, 4);

	_request.delete_by_task_id(6);
	ASSERT_EQ(_request._task_list.size() == 2, true);
}

TEST_F(HistoryRequestTest, delete_by_stream_id)
{
	cache_request_t what1;
	string str = "laifeng,tian tian test11111!";
    what1.generate_debug_header(str);
	_request.clear();
	_request.insert(1, what1, 3);

	cache_request_t what2;
	string str2 = "3434,tian tian test2222!";
    what2.generate_debug_header(str2);

	_request.insert(1, what2, 4);
	_request.delete_by_stream_id(1, what1);
	ASSERT_EQ(_request._task_list.size() == 1, true);
}

TEST_F(HistoryRequestTest, clear)
{
	cache_request_t what1;
	string str = "laifeng,tian tian test11111!";
    what1.generate_debug_header(str);
	_request.clear();
	ASSERT_EQ(_request._task_list.size() == 0, true);
	_request.insert(1, what1, 3);

	str = "err,ti34an tian test11112!";
	what1.generate_debug_header(str);
	_request.insert(2, what1, 4);

	str = "345,5656 tian test11113!";
	what1.generate_debug_header(str);
	_request.insert(3, what1, 5);
	ASSERT_EQ(_request._task_list.size() == 3, true);
	_request.clear();
	ASSERT_EQ(_request._task_list.size() == 0, true);
}

TEST_F(HistoryRequestTest, get_task_id)
{
	cache_request_t what1,what2;
	string str = "laifeng,tian tian test11111!";
    what1.generate_debug_header(str);
	_request.clear();
	_request.insert(1, what1, 1000);
	str = "laifeng,tian 4545 test11121!";
	what2.generate_debug_header(str);
	_request.insert(2, what2, 1001);
	str = "dd,ttttt tian test11113!";
	what1.generate_debug_header(str);
	_request.insert(3, what1, 1002);
	ASSERT_EQ(_request.get_task_id(2, what2) == 1001, true);
}

TEST_F(HistoryRequestTest, get_task_id_failed)
{
	cache_request_t what1;
	string str = "laifeng,tian tian test11111!";
    what1.generate_debug_header(str);
	_request.clear();
	_request.insert(1, what1, 1000);

	str = "laifeng,tttt55 tian test11113!";
	what1.generate_debug_header(str);
	_request.insert(2, what1, 1001);

	str = "ttttt,tian tian test11114!";
	what1.generate_debug_header(str);
	_request.insert(3, what1, 1002);

	cache_request_t what2;
	string str2 = "89898,tian tian test88888!";
    what2.generate_debug_header(str2);
	ASSERT_EQ(_request.get_task_id(3, what2) == 0, true);
}

TEST_F(HistoryRequestTest, delete_tasks)
{
	cache_request_t what1;
	string str = "ggggg,supper test11111!";
    what1.generate_debug_header(str);
	_request.clear();

	_request.insert(1, what1, 1000);
	str = "laifeng,supper test11113!";
	what1.generate_debug_header(str);
	_request.insert(2, what1, 1001);

	str = "iiiii,supper test11114!";
	what1.generate_debug_header(str);
	_request.insert(3, what1, 1002);

	TaskList list;
	list.push_back(1000);
	list.push_back(1001);
	list.push_back(1002);
	_request.delete_tasks(list);
	ASSERT_EQ(_request._task_list.size() == 0, true);
}

TEST_F(HistoryRequestTest, delete_tasks_failed)
{
	cache_request_t what1;
	string str = "fgfgf,supper test11111!";
    what1.generate_debug_header(str);
	_request.clear();
	_request.insert(1, what1, 1000);

	str = "laifeng,ggg test11112!";
	what1.generate_debug_header(str);
	_request.insert(2, what1, 1001);

	str = "trtrt,supper test11113!";
	what1.generate_debug_header(str);
	_request.insert(3, what1, 1002);

	TaskList list;
	list.push_back(1000);
	list.push_back(1003);
	list.push_back(1002);
	_request.delete_tasks(list);
	ASSERT_EQ(_request._task_list.size() == 1, true);
}

TEST_F(HistoryRequestTest, _is_need_delete)
{
	TaskList list;
	list.push_back(1000);
	list.push_back(1003);
	list.push_back(1002);
	ASSERT_EQ(_request._is_need_delete(1000, list) && \
		_request._is_need_delete(1003, list) && \
		_request._is_need_delete(1002, list) && \
		(!_request._is_need_delete(9000, list)), true);

}


class HistoryStreamTest : public testing::Test
{
protected:

	virtual void SetUp()
	{

	}

	virtual void TearDown()
	{

	}

public:
	HistoryStream _stream;
};

TEST_F(HistoryStreamTest, insert)
{
	_stream.clear();
	ASSERT_EQ(_stream.insert(1, 2) && \
		_stream._stream_task_list.size() == 1 && \
		_stream._stream_task_list.begin()->second.size() == 1, true);
}

TEST_F(HistoryStreamTest, insert_failed)
{
	_stream.clear();
	_stream.insert(1, 2);
	ASSERT_EQ(!_stream.insert(1, 2) && \
		_stream._stream_task_list.size() == 1 && \
		_stream._stream_task_list.begin()->second.size() == 1, true);
}

TEST_F(HistoryStreamTest, get_task_list)
{
	_stream.clear();
	_stream.insert(1, 2);
	_stream.insert(1, 3);
	_stream.insert(1, 4);
	_stream.insert(1, 5);
	_stream.insert(2, 6);

	TaskList list;
	ASSERT_EQ(_stream.get_task_list(1, list) && list.size() == 4, true);

}

TEST_F(HistoryStreamTest, get_task_list_failed)
{
	_stream.clear();
	_stream.insert(1, 2);
	_stream.insert(1, 3);
	_stream.insert(2, 6);

	TaskList list;
	ASSERT_EQ(!_stream.get_task_list(3, list) && list.size() == 0, true);
}


TEST_F(HistoryStreamTest, delete_stream)
{
	_stream.clear();
	_stream.insert(1, 2);
	_stream.insert(1, 3);
	_stream.insert(2, 6);
	_stream.insert(2, 7);
	_stream.delete_stream(1);
	ASSERT_EQ(_stream._stream_task_list.size() == 1 && \
		_stream._is_exist(2, 6) && \
		_stream._is_exist(2, 7), true);


}

TEST_F(HistoryStreamTest, delete_stream_failed)
{
	_stream.clear();
	_stream.insert(1, 2);
	_stream.insert(1, 3);
	_stream.delete_stream(2);
	ASSERT_EQ(_stream._stream_task_list.size() == 1, true);
}

TEST_F(HistoryStreamTest, delete_task)
{
	_stream.clear();
	_stream.insert(1, 2);
	_stream.insert(1, 3);
	_stream.delete_task(2);
	ASSERT_EQ(_stream._stream_task_list.size() == 1 && \
		_stream._is_exist(1, 3), true);

}

TEST_F(HistoryStreamTest, delete_task_failed)
{
	_stream.clear();
	_stream.insert(1, 2);
	_stream.insert(2, 3);
	_stream.delete_task(4);
	ASSERT_EQ(_stream._stream_task_list.size() == 2 && \
		_stream._is_exist(2, 3) && \
		_stream._is_exist(1, 2), true);
}

class HistoryTaskTest : public testing::Test
{
protected:

	virtual void SetUp()
	{

	}

	virtual void TearDown()
	{

	}

public:
	HistoryTask _task;
};

TEST_F(HistoryTaskTest, insert)
{
	_task.clear();
	ASSERT_EQ(_task.insert(1, 2) && _task._task_stream_list.size() == 1, true);
}

TEST_F(HistoryTaskTest, insert_failed)
{
	_task.clear();
	_task.insert(1, 2);
	ASSERT_EQ(!_task.insert(1, 2) && _task._task_stream_list.size() == 1, true);
}

TEST_F(HistoryTaskTest, delete_by_stream_id)
{
	_task.clear();
	_task.insert(1, 2);
	_task.insert(2, 3);
	_task.delete_by_stream_id(1);
	ASSERT_EQ(_task._task_stream_list.size() == 1, true);

}

TEST_F(HistoryTaskTest, delete_by_stream_id_failed)
{
	_task.clear();
	_task.insert(1, 2);
	_task.insert(2, 3);
	_task.insert(3, 4);
	_task.delete_by_stream_id(4);
	ASSERT_EQ(_task._task_stream_list.size() == 3, true);
}

TEST_F(HistoryTaskTest, delete_by_task_id)
{
	_task.clear();
	_task.insert(1, 2);
	_task.insert(2, 3);
	_task.insert(3, 4);
	_task.delete_by_task_id(3);
	ASSERT_EQ(_task._task_stream_list.size() == 2 && !_task._is_exist(3), true);
}

TEST_F(HistoryTaskTest, delete_by_task_id_failed)
{
	_task.clear();
	_task.insert(1, 2);
	_task.insert(2, 3);
	_task.insert(3, 4);
	_task.delete_by_task_id(1);
	ASSERT_EQ(_task._task_stream_list.size() == 3, true);
}

TEST_F(HistoryTaskTest, get_stream_id)
{
	_task.clear();
	_task.insert(100000, 20000);
	_task.insert(200000, 30000);
	_task.insert(300000, 40000);
	ASSERT_EQ(200000 == _task.get_stream_id(30000) && 300000 == _task.get_stream_id(40000), true);
}

TEST_F(HistoryTaskTest, get_stream_id_failed)
{
	_task.clear();
	_task.insert(100000, 20000);
	_task.insert(200000, 30000);
	_task.insert(300000, 40000);
	ASSERT_EQ(0 == _task.get_stream_id(80000), true);
}

TEST_F(HistoryTaskTest, delete_tasks)
{
	_task.clear();
	_task.insert(100000, 20000);
	_task.insert(200000, 30000);
	_task.insert(300000, 40000);
	TaskList list;
	list.push_back(20000);
	list.push_back(30000);
	list.push_back(40000);
	_task.delete_tasks(list);
	ASSERT_EQ(0 == _task._task_stream_list.size(), true);

}

TEST_F(HistoryTaskTest, _is_need_delete)
{
	_task.clear();
	_task.insert(100000, 20000);
	_task.insert(200000, 30000);
	_task.insert(300000, 40000);
	TaskList list;
	list.push_back(20000);
	list.push_back(30000);
	list.push_back(40000);
	ASSERT_EQ(_task._is_need_delete(40000, list) && \
		_task._is_need_delete(30000, list) && \
		(!_task._is_need_delete(80000, list)), true);
}


