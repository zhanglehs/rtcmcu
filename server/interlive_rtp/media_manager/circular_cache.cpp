/**
* @file circular_cache.cpp
* @brief implementation of circular_cache.h
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/11/20
*/

#include "circular_cache.h"
#include "cache_manager.h"

using namespace std;
using namespace fragment;

namespace media_manager
{
    CircularCache::CircularCache(StreamId_Ext& stream_id)
        :_last_push_time(0),
        _last_req_time(0),
        _temp_pushed_count(0),
        _just_pop_block_timestamp(-1),
        _stream_id(stream_id),
        _last_push_relative_timestamp_ms(0)
    {
        
    }

    int32_t CircularCache::push(BaseBlock* block)
    {
        if (block == NULL)
        {
            return 0;
        }

        if (_block_store.size() > 0
            && block->get_seq() <= _block_store.back()->get_seq())
        {
            WRN("block seq is too small, stream: %s, seq %d, max seq in store %d",
                _stream_id.unparse().c_str(), block->get_seq(), _block_store.back()->get_seq());
            return -1;
        }

        // push into _block_store
        _block_store.push_back(block);

        //if (block->is_key())
        //{
        //    _key_block_store.push_back(block);
        //}

        _add_index(block);

        TRC("push %s success, stream_id: %s, seq: %d, size: %d",
            block->get_type_s().c_str(), block->get_stream_id().unparse().c_str(), block->get_seq(), block->size());

        _temp_pushed_count++;
        if (_temp_pushed_count >= 100)
        {
            INF("pushed %d %s success, stream_id: %s, latest seq: %d",
                _temp_pushed_count, block->get_type_s().c_str(), block->get_stream_id().unparse().c_str(), block->get_seq());
            _temp_pushed_count = 0;
        }

        set_push_active();

        _last_push_relative_timestamp_ms = block->get_start_ts();

        return _block_store.size();
    }

    BaseBlock* CircularCache::get_by_seq(uint32_t seq, int32_t& status_code, bool return_next_if_error, bool only_check)
    {
        if (_block_store.size() == 0)
        {
            status_code = STATUS_NO_DATA;

            return NULL;
        }

        //printf("seq: %d min: %d max %d\n", seq, _block_store[0]->get_seq(), _block_store.back()->get_seq());

        if (seq < _block_store[0]->get_seq())
        {
            if (return_next_if_error)
            {
                status_code = STATUS_SUCCESS;
                if (!only_check)
                {
                    set_req_active();
                }
                return _block_store[0];
            }
            else 
            {
                status_code = STATUS_TOO_SMALL;
                return NULL;
            }
        }

        if (seq > _block_store.back()->get_seq())
        {
            status_code = STATUS_TOO_LARGE;
            return NULL;
        }

        int32_t idx = seq - _block_store.front()->get_seq();

        // data is wrong, binary search the whole store
        if (idx >= (int32_t)_block_store.size() || _block_store[idx]->get_seq() != seq)
        {
            int32_t low = 0;
            int32_t high = _block_store.size() - 1;
            int32_t mid = 0;

            while (low <= high)
            {
                mid = (low + high) / 2;
                if (_block_store[mid]->get_seq() < seq)
                {
                    low = mid + 1;
                }
                else if (_block_store[mid]->get_seq() > seq)
                {
                    high = mid - 1;
                }
                else
                {
                    break;
                }
            }

            if (_block_store[mid]->get_seq() != seq)
            {
                if (return_next_if_error && mid < (int32_t)_block_store.size() - 1)
                {
                    status_code = STATUS_SUCCESS;
                    if (!only_check)
                    {
                        set_req_active();
                    }
                    return _block_store[mid + 1];
                }
                else
                {
                    status_code = STATUS_NO_DATA;
                    return NULL;
                }
            }
            else
            {
                status_code = STATUS_SUCCESS;
                if (!only_check)
                {
                    set_req_active();
                }
                return _block_store[mid]; 
            }
        }
        else
        {
            status_code = STATUS_SUCCESS;
            if (!only_check)
            {
                set_req_active();
            }
            return _block_store[idx];
        }

        return NULL;
    }

    BaseBlock* CircularCache::get_next_by_seq(int32_t seq, int32_t& status_code, bool only_check)
    {
        return get_by_seq(seq + 1, status_code, true, only_check);
    }

    BaseBlock* CircularCache::get_by_ts(uint32_t timestamp, int32_t& status_code, bool return_next_if_error, bool only_check)
    {
        if (_block_store.size() == 0)
        {
            status_code = STATUS_NO_DATA;

            return NULL;
        }

        if (timestamp < _block_store[0]->get_start_ts())
        {
            if (return_next_if_error)
            {
                status_code = STATUS_SUCCESS;
                if (!only_check)
                {
                    set_req_active();
                }
                return _block_store[0];
            }
            else
            {
                status_code = STATUS_TOO_SMALL;
                return NULL;
            }
        }

        if (timestamp > _block_store.back()->get_end_ts())
        {
            status_code = STATUS_TOO_LARGE;
            return NULL;
        }

        // binary search the whole store

        int32_t low = 0;
        int32_t high = _block_store.size() - 1;
        int32_t mid = 0;

        while (low <= high)
        {
            mid = (low + high) / 2;
            if (_block_store[mid]->get_start_ts() < timestamp)
            {
                low = mid + 1;
            }
            else if (_block_store[mid]->get_start_ts() > timestamp)
            {
                high = mid - 1;
            }
            else
            {
                break;
            }
            mid = (low + high) / 2;
        }

        if (timestamp > _block_store[mid]->get_end_ts())
        {
            if (return_next_if_error && mid < (int32_t)_block_store.size() - 1)
            {
                status_code = STATUS_SUCCESS;
                if (!only_check)
                {
                    set_req_active();
                }
                return _block_store[mid + 1];
            }
            else
            {
                status_code = STATUS_NO_DATA;
                return NULL;
            }
        }
        else
        {
            status_code = STATUS_SUCCESS;
            if (!only_check)
            {
                set_req_active();
            }
            return _block_store[mid];
        }

        return NULL;
    }

    BaseBlock* CircularCache::get_next_by_ts(uint32_t timestamp, int32_t& status_code, bool only_check)
    {
        if (_block_store.size() == 0)
        {
            status_code = STATUS_NO_DATA;

            return NULL;
        }

        if (timestamp < _just_pop_block_timestamp)
        {
            status_code = STATUS_TOO_SMALL;
            return NULL;
        }

        else if (timestamp < _block_store.front()->get_start_ts())
        {
            status_code = STATUS_SUCCESS;
            if (!only_check)
            {
                set_req_active();
            }
            return _block_store.front();
        }

        if (timestamp > _block_store.back()->get_start_ts())
        {
            status_code = STATUS_TOO_LARGE;
            return NULL;
        }

        // binary search the whole store
        int32_t low = 0;
        int32_t high = _block_store.size() - 1;
        int32_t mid = 0;

        while (low <= high)
        {
            mid = (low + high) / 2;
            if (_block_store[mid]->get_start_ts() < timestamp)
            {
                low = mid + 1;
            }
            else if (_block_store[mid]->get_start_ts() > timestamp)
            {
                high = mid - 1;
            }
            else
            {
                break;
            }
            mid = (low + high) / 2;
        }

        if (mid < (int32_t)_block_store.size() - 1)
        {
            status_code = STATUS_SUCCESS;
            if (!only_check)
            {
                set_req_active();
            }
            return _block_store[mid + 1];
        }
        else
        {
            status_code = STATUS_NO_DATA;
            return NULL;
        }

        return NULL;
    }

    BaseBlock* CircularCache::get_latest(int32_t& status_code, bool only_check)
    {
        if (_block_store.size() == 0)
        {
            status_code = STATUS_NO_DATA;

            return NULL;
        }

        status_code = STATUS_SUCCESS;
        if (!only_check)
        {
            set_req_active();
        }
        return _block_store.back();
    }

    BaseBlock* CircularCache::get_latest_n(::int32_t& status_code, int32_t latest_n, bool only_check, bool is_key)
    {
        BaseBlock* block = NULL;
        uint32_t index = -1;
        if (_block_store.size() == 0)
        {
            status_code = STATUS_NO_DATA;
            return NULL;
        }
        else if ((int)_block_store.size() <= latest_n)
        {
            if (!only_check)
            {
                set_req_active();
            }
            block= front(status_code, only_check);
            index = 0; 
        }
        else // _block_store.size() > latest_n
        {
            if (!only_check)
            {
                set_req_active();
            }
            status_code = STATUS_SUCCESS;
            index = _block_store.size() - latest_n -1;
            block = _block_store[index];
        }

        if (!is_key)
        {
            return block;
        }

        //uint32_t size = _block_store.size();
        while (index > 0)
        {
            block = _block_store[index];
            if (block->is_key())
            {
                return block;
            }
            index -- ;
        }

        return block;
    }

    BaseBlock* CircularCache::get_latest_key(int32_t& status_code, bool only_check)
    {
        if (_block_store.size() == 0)
        {
            status_code = STATUS_NO_DATA;
            return NULL;
        }

        deque<BaseBlock*>::reverse_iterator it = _block_store.rbegin();

        for (; it != _block_store.rend(); it++)
        {
            BaseBlock* block = *it;
            if (block->is_key())
            {
                status_code = STATUS_SUCCESS;
                if (!only_check)
                {
                    set_req_active();
                }
                return block;
            }
        }

        status_code = STATUS_NO_DATA;
        return NULL;
    }

    BaseBlock* CircularCache::front(::int32_t& status_code, bool only_check)
    {
        if (_block_store.size() > 0)
        {
            status_code = STATUS_SUCCESS;
        }
        else
        {
            status_code = STATUS_NO_DATA;
        }
        return _block_store.front();
    }

    BaseBlock* CircularCache::back(::int32_t& status_code, bool only_check)
    {
        if (_block_store.size() > 0)
        {
            status_code = STATUS_SUCCESS;
        }
        else
        {
            status_code = STATUS_NO_DATA;
        }
        return _block_store.back();
    }

    uint32_t CircularCache::size()
    {
        return _block_store.size();
    }

    StreamId_Ext& CircularCache::stream_id()
    {
        return _stream_id;
    }

    int32_t CircularCache::pop_front(bool delete_flag)
    {
        if (_block_store.size() == 0)
        {
            return -1;
        }

        BaseBlock* block = _block_store.front();

        _just_pop_block_timestamp = block->get_start_ts();

        if (delete_flag == true)
        {
            delete _block_store.front();
        }

        _block_store.pop_front();

        return _block_store.size();
    }

    time_t CircularCache::reset_push_active()
    {
        _last_push_time = 0;
        return _last_push_time;
    }

    time_t CircularCache::reset_req_active()
    {
        _last_req_time = 0;
        return _last_req_time;
    }

    time_t CircularCache::set_push_active()
    {
        _last_push_time = time(NULL);
        return _last_push_time;
    }

    time_t CircularCache::set_req_active()
    {
        _last_req_time = time(NULL);
        return _last_req_time;
    }

    time_t CircularCache::get_push_active_time()
    {
        return _last_push_time;
    }

    time_t CircularCache::get_req_active_time()
    {
        return _last_req_time;
    }

    uint64_t CircularCache::get_last_push_relative_timestamp_ms()
    {
        return _last_push_relative_timestamp_ms;
    }

    int32_t CircularCache::_add_index(fragment::BaseBlock* block)
    {
        return 0;
    }

    int32_t CircularCache::_remove_index(fragment::BaseBlock* block)
    {
        return 0;
    }

    BaseBlock* CircularCache::_delete(BaseBlock* block)
    {
        delete block;
        return block;
    }

    CircularCache::~CircularCache()
    {
        CircularCache::clear();
    }

    int32_t CircularCache::clear()
    {
        int32_t size = 0;
        size = _block_store.size();

        for (int32_t i = 0; i < size; i++)
        {
            if (_block_store[i] != NULL)
            {
                BaseBlock* block = _block_store[i];
                delete block;
            }
        }

        _block_store.clear();

        _last_push_time = 0;
        _last_req_time = 0;

        _just_pop_block_timestamp = -1;
        _temp_pushed_count = 0;

        DBG("CircularCache::clear(), stream_id: %s", _stream_id.unparse().c_str());

        return 0;
    }
}
