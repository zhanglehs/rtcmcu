/**
* @file smart_memory_pool.h
* @brief To realize the automatic recovery method based on the memory pool. 
*        Requirements of external timing calls the timer function.
*		 The best call once per second. External use the memory unit, 
*		 the memory unit state can MBSTATE_FREE. 
*		 The size of the memory unit and memory pool size is specified 
*		 in the create function, process exit to release the memory called destory.
* @author renzelong
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>renzelong@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/06/20
*
*/

#ifndef __SMART_MEMORY_POOL_H__
#define __SMART_MEMORY_POOL_H__

#include <list>
#include "error_defs.h"


typedef enum MBState
{
    MBSTATE_FREE,
    MBSTATE_OCCUPIED,
    MBSTATE_UNKOWN,
}MBState;

#pragma pack(1)
template<typename T>
class MemoryBlock
{
public:
    uint8_t state; /*0:free,1:Occupied*/
    T item;
};
#pragma pack()

template<typename T>
class SmartMemoryPool
{
public:

    SmartMemoryPool()
    {
        _recy_time_last = time(NULL);
        _recy_time_len = 10;//10 seconds
    }

    ~SmartMemoryPool()
    {
        destory();
    }

    result_t create(uint32_t item_num, uint32_t item_size, time_t recy_time_len = 10)
    {
        if (item_size <= 0)
        {
            return ERR_CODE_PARAMS_INVALID;
        }
        for (uint32_t i = 0; i < item_num; i++)
        {
            MemoryBlock<T>* block = (MemoryBlock<T>*)mmalloc(sizeof(MemoryBlock<T>) + item_size + 4);
            if (NULL == block)
            {
                return ERR_CODE_MEMORY_NOT_ENOUGH;
            }
            block->state = MBSTATE_FREE;
            _available_items.push_back(block);
        }
        return ERR_CODE_OK;
    }

    result_t destory()
    {
        typename ItemList::iterator iter = _available_items.begin();
        while (iter != _available_items.end())
        {
            MemoryBlock<T>* block = *iter;
            _available_items.erase(iter++);
            if (NULL != block)
            {
                mfree(block);
                block = NULL;
            }
        }

        iter = _occupied_items.begin();
        while (iter != _occupied_items.end())
        {
            MemoryBlock<T>* block = *iter;
            _occupied_items.erase(iter++);
            if (NULL != block)
            {
                mfree(block);
                block = NULL;
            }
        }

        return ERR_CODE_OK;
    }

    MemoryBlock<T> * malloc()
    {
        if (_available_items.size() > 0)
        {
            MemoryBlock<T>* block = *_available_items.begin();
            _available_items.pop_front();
            block->state = MBSTATE_OCCUPIED;
            _occupied_items.push_back(block);
            return block;
        }
        return NULL;
    }

    void free(MemoryBlock<T>** block)
    {
        if (block && *block)
        {
            _remove_item(_occupied_items, *block);
            (*block)->state = MBSTATE_FREE;
            _available_items.push_back(*block);
        }
        *block = NULL;
    }

    size_t get_available_size()
    {
        return  _available_items.size();
    }

    void timer()
    {
        time_t now = time(NULL);
        if (now - _recy_time_last >= _recy_time_len)
        {
            _recycling();
            _recy_time_last = now;
        }
    }

    
private:
    void _recycling()
    {
        typename ItemList::iterator iter = _occupied_items.begin();
        while (iter != _occupied_items.end())
        {
            MemoryBlock<T>* block = (*iter);
            if (block && MBSTATE_FREE == block->state)
            {
                _occupied_items.erase(iter++);
                _available_items.push_back(block);
            }
            else
            {
                iter++;
            }
        }
    }

    typedef std::list<  MemoryBlock<T> * > ItemList;

    void _remove_item(ItemList & list, const MemoryBlock<T>* block)
    {
        typename ItemList::iterator iter = list.begin();
        while (iter != list.end())
        {
            if (block == (*iter))
            {
                list.erase(iter++);
            }
            else
            {
                iter++;
            }
        }
    }

    void _remove_item(ItemList & list, const MBState state)
    {
        typename ItemList::iterator iter = list.begin();
        while (iter != list.end())
        {
            MemoryBlock<T>* block = (*iter);
            if (block && state == block->state)
            {
                list.erase(iter++);
            }
            else
            {
                iter++;
            }
        }
    }

private:

    ItemList _available_items;
    ItemList _occupied_items;
    time_t _recy_time_last;
    time_t _recy_time_len;
};

#endif/*__SMART_MEMORY_POOL_H__*/

