/**
* @file cache_manager_test.cpp
* @brief this is unit tests for cache_manager and fragment
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/05/29
* @see  fragment.h \n
*		cache_manager.h
*/

#define protected public
#define private public

#include <string.h>

#include <iostream>
#include <fstream>
#include "gtest/gtest.h"
#include "util/unit_test_defs.h"
#include "interlive/cache_manager.h"

#define DBG(...) printf(...)
#define WRN(...) printf(...)
#define ERR(...) printf(...)
#define INF(...) printf(...)
#define TRC(...) printf(...)

using namespace std;
using namespace fragment;
using namespace media_manager;

int start_stream_flag = 0;
int stop_stream_flag = 0;
int add_stream_to_tracker_flag = 0;
int del_stream_from_tracker_flag = 0;


int32_t backend_start_stream_v2(uint32_t stream_id, const cache_request_header & request_header)
{
	start_stream_flag++;
	return 0;
}

int32_t backend_stop_stream_v2(uint32_t stream_id, const cache_request_header & request_header)
{
	stop_stream_flag++;
	return 0;
}

void backend_add_stream_to_tracker(uint32_t stream_id, int level)
{
	add_stream_to_tracker_flag++;
	return;
}

void backend_del_stream_from_tracker(uint32_t stream_id, int level)
{
	del_stream_from_tracker_flag++;
	return;
}

class uploader_cache_manager_test : public testing::Test
{
protected:
	static void SetUpTestCase()
	{

	}
	static void TearDownTestCase()
	{

	}

	virtual void SetUp()
	{
		cache = new FlvCacheManager(MODULE_TYPE_UPLOADER);
		uploader_cache = cache;
		player_cache = cache;

		uint8_t	cache_ratetype_config[RateTypeCount];
		stream_id = 127;

		int i;
		for (i = 0; i < (int)RateTypeCount; i++)
		{
			cache_ratetype_config[i] = 0x00;
		}

		rate_type = RateTypeCount - 1;
		cache_ratetype_config[rate_type] = 0x01;

		//uploader_cache->init_stream(stream_id, cache_ratetype_config);  //#todo delete
		uploader_cache->init_stream_v2(stream_id, cache_ratetype_config);

		string input_file_name = "data/test2.flv";

		ifstream input_file(input_file_name.c_str(), ios::binary);
		input_file.seekg(0, ios::end);
		streampos pos = input_file.tellg();
		input_file.seekg(0, ios::beg);
		size = pos;

		flv_file_content = (uint8_t*)malloc(size);

		input_file.read((char*)flv_file_content, size);
		input_file.close();
	}

	virtual void TearDown()
	{
		delete cache;
	}

public:
	FlvCacheManager* cache;
	UploaderCacheManagerInterface* uploader_cache;
	PlayerCacheManagerInterface* player_cache;

	uint8_t rate_type;

	uint8_t* flv_file_content;
	int size;
	uint32_t stream_id;
};


TEST_F(uploader_cache_manager_test, set_get_flv_header)
{
	int push_size = 0;
	int total_push_size = 0;
	push_size = uploader_cache->set_flv_header(stream_id, (flv_header*)flv_file_content, true);
	total_push_size += push_size;

	flv_header* input_flv_header = (flv_header*)flv_file_content;
	int tag_offset = 4;
	while (1)
	{
		flv_tag* tag = (flv_tag*)(input_flv_header->data + tag_offset);

		if ((FLV_TAG_AUDIO == tag->type && !flv_is_aac0(tag->data))
			|| (FLV_TAG_VIDEO == tag->type && !flv_is_avc0(tag->data)))
		{
			break;
		}

		tag_offset += flv_get_datasize(tag->datasize) + sizeof(flv_tag)+4;
	}

	int _flv_header_len = sizeof(flv_header)+tag_offset;

	int len = 0;
	int status_code = 0;

	flv_header* header = player_cache->get_flv_header(stream_id, rate_type, len, status_code);

	ASSERT_EQ(_flv_header_len, len);
	ASSERT_EQ(status_code, STATUS_SUCCESS);
	ASSERT_EQ(memcmp(input_flv_header, header, _flv_header_len), 0);

	header = player_cache->get_flv_header(stream_id + 1, rate_type, len, status_code);

	ASSERT_EQ(header == NULL, true);
	ASSERT_EQ(0, len);
	ASSERT_EQ(status_code, STATUS_NO_THIS_STREAM);

	header = player_cache->get_flv_header(stream_id, rate_type - 1, len, status_code);

	ASSERT_EQ(header == NULL, true);;
	ASSERT_EQ(0, len);
	ASSERT_EQ(status_code, STATUS_NO_THIS_RATE);
}

TEST_F(uploader_cache_manager_test, set_flv_tag)
{
	int push_size = 0;
	int total_push_size = 0;
	push_size = uploader_cache->set_flv_header(stream_id, (flv_header*)flv_file_content, true);
	total_push_size += push_size;

	int i;
	int push_block_size = 0;
	for (i = 0; i < 22999; i++)
	{
		push_size = uploader_cache->set_flv_tag(stream_id,
			(flv_tag*)(flv_file_content + total_push_size), true);
		total_push_size += push_size;
		if (total_push_size == size)
		{
			break;
		}
		push_block_size += push_size;
	}

	int status_code = 0;

	vector<FLVBlockCircularCache*> block_cache_vec = cache->_block_cache_map[stream_id];

	int block_cache_size = block_cache_vec[rate_type]->_block_store.size();

	int block_total_size = 0;
	for (i = 0; i < block_cache_size; i++)
	{
		block_total_size += block_cache_vec[rate_type]->get_block_by_seq(i, status_code)->get_payload_size();
	}

	ASSERT_EQ(block_total_size, push_block_size - push_size);
}

TEST_F(uploader_cache_manager_test, generate_block)
{
	int push_size = 0;
	int total_push_size = 0;
	push_size = uploader_cache->set_flv_header(stream_id, (flv_header*)flv_file_content, true);
	total_push_size += push_size;

	int i;
	int push_block_size = 0;
	for (i = 0; i < 22999; i++)
	{
		flv_tag* tag = (flv_tag*)(flv_file_content + total_push_size);

		int timestamp = flv_get_timestamp(tag->timestamp);

		// remove several blocks
		if (timestamp / 1000 == 20
			|| timestamp / 1000 == 120
			|| timestamp / 1000 == 220)
		{
			push_size = flv_get_datasize(tag->datasize) + 15;
			total_push_size += push_size;
			continue;
		}

		push_size = uploader_cache->set_flv_tag(stream_id,
			(flv_tag*)(flv_file_content + total_push_size), true);
		total_push_size += push_size;

		if (total_push_size == size)
		{
			break;
		}

		if (i > 20000)
		{
			if (cache->_block_cache_map[stream_id][rate_type]->_temp_tag_store.size() == 1)
			{
				break;
			}
		}

		push_block_size += push_size;
	}

	int status_code = 0;

	vector<FLVBlockCircularCache*> block_cache_vec = cache->_block_cache_map[stream_id];

	int block_cache_size = block_cache_vec[rate_type]->_block_store.size();

	FLVBlock* block;

	int block_total_size = 0;
	for (i = 0; i < block_cache_size; i++)
	{
		block_total_size += block_cache_vec[rate_type]->get_block_by_seq(i, status_code)->get_payload_size();
	}

	ASSERT_EQ(block_total_size, push_block_size);

	for (i = 0; i < block_cache_size; i++)
	{
		if (i * CutIntervalForBlock / 1000 == 20
			|| i * CutIntervalForBlock / 1000 == 120
			|| i * CutIntervalForBlock / 1000 == 220)
		{
			block = block_cache_vec[rate_type]->get_block_by_seq(i, status_code);
			ASSERT_EQ((block != NULL), true);
			ASSERT_EQ(block->get_payload_size(), 0);
		}
	}
}

TEST_F(uploader_cache_manager_test, generate_fragment)
{
	int push_size = 0;
	int total_push_size = 0;
	push_size = uploader_cache->set_flv_header(stream_id, (flv_header*)flv_file_content, true);
	total_push_size += push_size;

	int i;
	int push_block_size = 0;
	for (i = 0; i < 22999; i++)
	{
		flv_tag* tag = (flv_tag*)(flv_file_content + total_push_size);

		int timestamp = flv_get_timestamp(tag->timestamp);

		// remove several fragments
		if (timestamp / 100000 == 2
			|| timestamp / 100000 == 12
			|| timestamp / 100000 == 22)
		{
			push_size = flv_get_datasize(tag->datasize) + 15;
			total_push_size += push_size;
			continue;
		}

		push_size = uploader_cache->set_flv_tag(stream_id,
			(flv_tag*)(flv_file_content + total_push_size), true);
		total_push_size += push_size;

		if (total_push_size == size)
		{
			break;
		}

		if (i > 20000)
		{
	//		if (cache->_block_cache_map[stream_id][rate_type]->_generate_latest_fragment == 1)
			{
				break;
			}
		}

		push_block_size += push_size;
	}

	int status_code = 0;

	vector<FLVBlockCircularCache*> block_cache_vec = cache->_block_cache_map[stream_id];

	int block_cache_size = block_cache_vec[rate_type]->_block_store.size();

	FLVBlock* block;

	int block_total_size = 0;
	for (i = 0; i < block_cache_size; i++)
	{
		block_total_size += block_cache_vec[rate_type]->get_block_by_seq(i, status_code)->get_payload_size();
	}

	ASSERT_EQ(block_total_size, push_block_size - push_size);

	for (i = 0; i < block_cache_size; i++)
	{
		if (i * CutIntervalForBlock / 1000 == 20
			|| i * CutIntervalForBlock / 1000 == 120
			|| i * CutIntervalForBlock / 1000 == 220)
		{
			block = block_cache_vec[rate_type]->get_block_by_seq(i, status_code);
			ASSERT_EQ((block != NULL), true);
			ASSERT_EQ(block->get_payload_size(), 0);
		}
	}
}
