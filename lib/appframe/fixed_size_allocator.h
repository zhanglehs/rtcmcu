#ifndef _FIXED_SIZE_ALLOCATOR_H_
#define _FIXED_SIZE_ALLOCATOR_H_

#include <sys/uio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>

class FixedSizeAllocator
{
    union Obj
    {
        Obj* _free_list_link;
        char _client_data[1];    /* The client sees this.*/
    };

public:
    FixedSizeAllocator(int size, int max_bytes, int capacity)
            :MAX_BYTES(max_bytes),
            _base_ptr(NULL),
            _n(size),
            _capacity(capacity)
    {
        _free_num = _n_objs = _capacity / _n;
    }

    FixedSizeAllocator()
            :MAX_BYTES(0),
            _base_ptr(NULL),
            _capacity(0),
            _free_num(0)
    {

    }
    
    ~FixedSizeAllocator()
    {
        if (_base_ptr)
        {
            ::free(_base_ptr);
            _base_ptr = NULL;
        }
    }

    int32_t init()
    {
        assert(_capacity > 0);
        _base_ptr = (char *)malloc(_capacity);
        if (_base_ptr == NULL)
        {
            return -1;
        }
            
        _free_head._free_list_link = (Obj *)( _base_ptr);
        for (int i = 0; i < _n_objs - 1; i++)
        {
            ((Obj *)(_base_ptr + i * _n))->_free_list_link = (Obj *)(_base_ptr + (i + 1) * _n);
        }

        ((Obj *)(_base_ptr + (_n_objs - 1) * _n))->_free_list_link = 0;

        _small_n[0] = 24;
        _small_free_num[0] = 0;
        int power2 = 32;
        for (int i = 1; i < 6; i++)
        {
            _small_n[i] = power2;
            _small_free_num[i] = 0;
            power2 *= 2;
        }

        return 0;
    }

    inline int size() const
    {
        return _n; 
    }

    inline int capacity() const
    { 
        return _capacity;
    }

    inline int allocated_blocks() const
    { 
        return _n_objs - _free_num;
    }

    inline int free_blocks() const
    {
        return _free_num;
    }
    
    void* allocate()
    {
        Obj* free_node = _free_head._free_list_link;
        if (free_node == 0)
        {
            return NULL;
        }
        else
        {
            _free_head._free_list_link = free_node->_free_list_link;
            _free_num--;

            return free_node;
        }     
    }

    int allocate(iovec* iov, int iov_num)
    {
        if (iov_num > _free_num)
        {
            return -1;
        }

        for (int i = 0; i < iov_num; i++)
        {
            iov[i].iov_base = allocate();
            iov[i].iov_len = 0;
        }

        return 0;
    }

    int allocate(void** iov, int iov_num)
    {
        if (iov_num > _free_num)
        {
            return -1;
        }

        for (int i = 0; i < iov_num; i++)
        {
            iov[i] = allocate();
        }

        return 0;
    }

    int allocate(int capacity, iovec* iov, int& iov_num)
    {
        int block_num = capacity / _n;
        if (capacity % _n != 0) 
        {
            block_num++;
        }

        if (block_num > iov_num)
        {
            return -1;
        }

        int rv = allocate(iov, block_num);
        if (rv < 0)
        {
            return -1;
        }

        iov_num = block_num;
        return 0;
    }

    /*
     * we add this function to allocate pages and save the start addresses
     * of each page at the spaces of last page (if enough) or a new page (if not)
     * Michael 2007/12/25
     */
    int allocate_block(void*& base, uint32_t total_block_num, uint32_t block_size)
    {
        uint32_t block_per_page = _n / block_size;
        uint32_t page_num = total_block_num / block_per_page;
        uint32_t last_page_used = total_block_num % block_per_page;

        if (last_page_used != 0)
        {
            page_num++;
        }
        if (page_num > (uint32_t)_free_num)
        {
            return -1;
        }

        uint32_t last_page_free_size = _n - last_page_used * block_size;
        if (last_page_free_size >= (page_num + 1) * sizeof(void *) && last_page_used != 0)
        {
            void* curr = allocate();
            if (curr == NULL)
            {
                return -1;
            }
            char* block_ptr = (char *)curr + last_page_used * block_size + sizeof(void *) * page_num;
            std::memset(block_ptr, 0, sizeof(void *));
            block_ptr -= sizeof(void *);
            std::memcpy(block_ptr, &curr, sizeof(void *));
            for (uint32_t i = 2; i <= page_num; i++)
            {
                curr = allocate();
                if (curr == NULL)
                {
                    return -1;
                }
                block_ptr -= sizeof(void *);
                std::memcpy(block_ptr, &curr, sizeof(void *));
            }
            
            base = (void *)block_ptr;
            return 0;
        }
        else if (_n >= (int)((page_num + 2) * sizeof(void *)))
        {
            void* curr = allocate();
            if (curr == NULL)
            {
                return -1;
            }
            char* block_ptr = (char *)curr + sizeof(void *) * (page_num + 1);
            std::memset(block_ptr, 0, sizeof(void *));
            block_ptr -= sizeof(void *);
            std::memcpy(block_ptr, &curr, sizeof(void *));
            for (uint32_t i = 1; i <= page_num; i++)
            {
                curr = allocate();
                if (curr == NULL)
                {
                    return -1;
                }
                block_ptr -= sizeof(void *);
                std::memcpy(block_ptr, &curr, sizeof(void *));
            }

            base = (void *)block_ptr;
            return 0;
        }

        return -1;
    }   

    /*
     * we add this function to support allocate small pages in a page
     * Michael 2007/12/27
     */
    int get_index(uint32_t capacity)
    {
        int num_of_page = (capacity - 1) / _n;
        if (num_of_page == 0)
        {
            return 0;
        }
        if (num_of_page >= 1 && num_of_page < 3)
        {
            return 1;
        }
        if (num_of_page >= 3 && num_of_page < 11)
        {
            return 2;
        }
        if (num_of_page >= 11 && num_of_page < 27)
        {
            return 3;
        }
        if (num_of_page >= 27 && num_of_page < 59)
        {
            return 4;
        }
        if (num_of_page >= 59 && num_of_page < 123)
        {
            return 5;
        }

        return -1;
    }

    void* allocate_small(uint32_t capacity)
    {
        // get small free node index
        int index = get_index(capacity);
        assert(index >= 0);
        Obj* small_free_node;
        if (_small_free_num[index] == 0)
        {
            small_free_node = (Obj *)allocate();
            if (small_free_node == NULL)
            {
                return NULL;
            }
            _small_free_head[index]._free_list_link = (Obj *)((char *)small_free_node + _small_n[index]);
            for (int i = 1; i < _n / _small_n[index] - 1; i++)
            {
                ((Obj *)((char *)small_free_node + i * _small_n[index]))->_free_list_link = 
                    (Obj *)((char *)small_free_node + (i + 1) * _small_n[index]);
            }
            ((Obj *)((char *)small_free_node + (_n / _small_n[index] - 1) * _small_n[index]))->_free_list_link = 0;
            _small_free_num[index] = _n / _small_n[index] - 1;
            return small_free_node;
        }
        else
        {
            small_free_node = _small_free_head[index]._free_list_link;
            if (small_free_node == 0)
            {
                return NULL;
            }
            else
            {
                _small_free_head[index]._free_list_link = small_free_node->_free_list_link;
                _small_free_num[index]--;

                return small_free_node;
            }     
        }
    }

    /* 
     * we add this function to support allocate n-continuous pages. 
     * Michael 2007/11/12
     */
    void* allocate_continuous(int capacity)
    {
        int block_num = capacity / _n;
        if (capacity % _n != 0)
        {
            block_num++;
        }

        if (block_num == 1)
        {
            return allocate();
        }

        if (_free_num < block_num)
        {
            return NULL;
        }

        Obj* free_node = _free_head._free_list_link;
        
        if (free_node == NULL)
        {
            return NULL;
        }

        int cblock_num = 1;
        int failure_num = 0;
        int max_failure_num = 100;
        Obj* next_free_node;
        Obj* start_node = free_node;
        Obj* start_node_pre = &_free_head;

        while (cblock_num < block_num && failure_num < max_failure_num)
        {
            next_free_node = free_node->_free_list_link;
            if (next_free_node == NULL)
            {
                return NULL;
            }
            if ((char *)free_node + _n == (char *)next_free_node)
            {
                cblock_num++;
                free_node = next_free_node;
            }
            else
            {
                cblock_num = 1;
                failure_num++;
                start_node_pre = free_node;
                free_node = next_free_node;
                start_node = next_free_node;
            }
        }

        if (failure_num == max_failure_num)
        {
            return NULL;
        }

        start_node_pre->_free_list_link = free_node->_free_list_link;
        _free_num -= block_num;

        return start_node;
    }
        
    void deallocate(void* block)
    {
        if (block == NULL)
        {
            return;
        }
        
        /*
         * we use the following deallocate function to guarantee that
         * the blocks with lower addresses are in front ot the blocks
         * with higher addresses
         */
        Obj* tmp = &_free_head;
        while (tmp->_free_list_link != NULL && (char *)(tmp->_free_list_link) < (char *)block)
        {
            tmp = tmp->_free_list_link;
        }
        
        ((Obj *)(block))->_free_list_link = tmp->_free_list_link;
        tmp->_free_list_link = ((Obj*)(block));

        /*
        ((Obj*)(block))->_free_list_link = _free_head._free_list_link;
        _free_head._free_list_link = ((Obj*)(block));
        */

        _free_num++;
    }

    void deallocate_small(void* smallblock, uint32_t capacity)
    {
        if (smallblock == NULL)
        {
            return;
        }
        int index = get_index(capacity);

        Obj* tmp = &_small_free_head[index];
        while (tmp->_free_list_link != NULL && (char *)(tmp->_free_list_link) < (char *)smallblock)
        {
            tmp = tmp->_free_list_link;
        }
        
        ((Obj *)(smallblock))->_free_list_link = tmp->_free_list_link;
        tmp->_free_list_link = ((Obj*)(smallblock));

        /*
        ((Obj*)(block))->_free_list_link = _free_head._free_list_link;
        _free_head._free_list_link = ((Obj*)(block));
        */

        _small_free_num[index]++;
    }

    void deallocate_block(void* base)
    {
        char* curr = (char *)base;
        while (*((int *)(curr)) != 0)
        {
            deallocate(curr);
            curr += sizeof(void *);
        }
    }

    /* 
     * we add to support deallocate n-continuous blocks. 
     * Michael 2007/11/12
     */
    void deallocate_continuous(void* block, int capacity)
    {
        int block_num = capacity / _n;
        if (capacity % _n != 0)
        {
            block_num++;
        }

        if (block_num == 1)
        {
            deallocate(block);
            return;
        }
        
        /*
         * use the following deallocate method to guarantee that
         * the blocks with lower addresses are in front ot the blocks
         * with higher addresses
         */
        Obj* tmp = &_free_head;
        while (tmp->_free_list_link != NULL && (char *)(tmp->_free_list_link) < (char *)block)
        {
            tmp = tmp->_free_list_link;
        }
        
        ((Obj *)((char *)(block) + (block_num - 1) * _n))->_free_list_link = tmp->_free_list_link;
        tmp->_free_list_link = ((Obj*)(block));

        for (int i = block_num - 2; i >= 0; i--)
        {
            ((Obj *)((char *)(block) + i * _n))->_free_list_link = ((Obj *)((char *)(block) + (i + 1) * _n));
        }

        _free_num += block_num;
    }
    
    void deallocate(iovec* iov, int iov_num)
    {
        for (int i = 0; i < iov_num; i++)
        {
            deallocate(iov[i].iov_base);
        }
    }

    void deallocate(void** iov, int iov_num)
    {
        for (int i = 0; i < iov_num; i++)
        {
            deallocate(iov[i]);
        }
    }
    
public:
    const int MAX_BYTES;

private:
    char* _base_ptr;
    int   _n;
    int   _n_objs;
    int   _capacity;
    Obj   _free_head;
    int   _free_num;
    int   _small_n[6];
    Obj   _small_free_head[6];
    int   _small_free_num[6];
};

#endif

