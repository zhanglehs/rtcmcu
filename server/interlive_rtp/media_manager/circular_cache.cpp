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
    _stream_id(stream_id) {
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

    TRC("push %s success, stream_id: %s, seq: %d, size: %d",
      block->get_type_s().c_str(), block->get_stream_id().unparse().c_str(), block->get_seq(), block->size());

    set_push_active();

    return _block_store.size();
  }

  BaseBlock* CircularCache::get_by_seq(uint32_t seq, int32_t& status_code)
  {
    if (_block_store.size() == 0) {
      status_code = STATUS_NO_DATA;
      return NULL;
    }

    //printf("seq: %d min: %d max %d\n", seq, _block_store[0]->get_seq(), _block_store.back()->get_seq());

    if (seq < _block_store[0]->get_seq())
    {
      status_code = STATUS_TOO_SMALL;
      return NULL;
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
        status_code = STATUS_NO_DATA;
        return NULL;
      }
      else
      {
        status_code = STATUS_SUCCESS;
        set_req_active();
        return _block_store[mid];
      }
    }
    else
    {
      status_code = STATUS_SUCCESS;
      set_req_active();
      return _block_store[idx];
    }

    return NULL;
  }

  BaseBlock* CircularCache::get_latest(int32_t& status_code)
  {
    if (_block_store.size() == 0)
    {
      status_code = STATUS_NO_DATA;

      return NULL;
    }

    status_code = STATUS_SUCCESS;
    set_req_active();
    return _block_store.back();
  }

  BaseBlock* CircularCache::get_latest_key(int32_t& status_code)
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
        set_req_active();
        return block;
      }
    }

    status_code = STATUS_NO_DATA;
    return NULL;
  }

  BaseBlock* CircularCache::front(::int32_t& status_code)
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

  BaseBlock* CircularCache::back(::int32_t& status_code)
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

    DBG("CircularCache::clear(), stream_id: %s", _stream_id.unparse().c_str());

    return 0;
  }
}
