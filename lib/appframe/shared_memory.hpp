/**
 * \file
 * \brief 又一个共享内存包装类...
 */
#ifndef _SHARED_MEMORY_HPP_
#define _SHARED_MEMORY_HPP_

#include <errno.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>  	  //O_CREAT, O_*...
#include <sys/shm.h>
#include <unistd.h> 	  //ftruncate, close
#include <semaphore.h>  //sem_t* family, SEM_VALUE_MAX
#include <sys/stat.h>     //mode_t, S_IRWXG, S_IRWXO, S_IRWXU,
#include <string>


/*!\file
   Describes SharedMemory class
*/



template <std::size_t OrigSize, std::size_t RoundTo>
struct ct_rounded_size
{
    enum
    {
        value = ((OrigSize - 1) / RoundTo + 1) * RoundTo
    };
};

/*!A class that wraps basic shared memory management*/
class SharedMemory
{
public:

    class segment_info_t
    {
        friend class SharedMemory;
    public:
        /*!Returns pointer to the shared memory fragment
           the user can overwrite*/
        void* get_user_ptr() const;
        /*!Returns the size of the shared memory fragment
           the user can overwrite*/
        std::size_t get_user_size()const;
    private:
        std::size_t ref_count;
        std::size_t real_size;
    };

    /*!Initializes members. Does not throw*/
    SharedMemory();

    /*!Calls close. Does not throw*/
    ~SharedMemory();

    /*!Creates a shared memory segment with name "name", with size "size".
       User can specify the mapping address. Never throws.*/
    bool create(const char* name, size_t size, const void* addr = 0);
    bool create(key_t key, size_t size, const void* addr = 0);

    /*!Creates a shared memory segment with name "name", with size "size".
       User can also specify the mapping address in "addr". Never throws.
       It also executes the functor "func" atomically if the segment is created.
       Functor has must have the following signature:
       bool operator()(const segment_info_t * info, bool created) const

       "info" is an initialized segment info structure, and "created"
       must be "true". If the functor returns "false", an error is supposed
       and segment will be closed. The functor must not throw.
       */
    template <class Func>
    bool create_with_func(const char* name, size_t size, Func func, const void* addr = 0);
    template <class Func>
    bool create_with_func(key_t key, size_t size, Func func, const void* addr = 0);

    /*!Opens previously created shared memory segment with name "name",
       and size "size". If the memory segment was not previously created,
       the function return false. User can specify the mapping address.
       Never throws.*/
    bool open(const char* name, size_t size, const void* addr = 0);
    bool open(key_t key, size_t size, const void* addr = 0);

    /*!Opens previously created shared memory segment with name "name",
       and size "size". If the memory segment was not previously created,
       the function returns "false". User can specify the mapping address.
       Never throws.
       It also executes the functor "func" atomically if the segment is opened.
       Functor has must have the following signature:
       bool operator()(const segment_info_t * info, bool created) const

       "info" is an initialized segment info structure, and "created"
       must be "false". If the functor returns "false", an error is supposed
       and segment will be closed. The functor must not throw.*/
    template <class Func>
    bool open_with_func(const char* name, size_t size, Func func, const void* addr = 0);
    template <class Func>
    bool open_with_func(key_t key, size_t size, Func func, const void* addr = 0);

    /*!Creates a shared memory segment with name "name", and size "size" if
       the shared memory was not previously created. If it was previously
       created it tries to open it. User can specify the mapping address.
       Never throws.*/
    bool open_or_create(const char* name, size_t size, const void* addr = 0);
    bool open_or_create(key_t key, size_t size, const void* addr = 0);

    /*!Creates a shared memory segment with name "name", and size "size" if
       the shared memory was not previously created. If it was previously
       created it tries to open it. User can specify the mapping address.
       Never throws.
       It also executes the functor "func" atomically if the segment is
       created or opened. Functor has must have the following signature:
       bool operator()(const segment_info_t * info, bool created) const

       "info" is an initialized segment info structure, and "created"
       will be "true" if the segment was created or "false" if the segment was
       opened. If the functor returns "false", an error is supposed
       and segment will be closed. The functor must not throw.*/
    template <class Func>
    bool open_or_create_with_func(const char* name, size_t size, Func func, const void* addr = 0);
    template <class Func>
    bool open_or_create_with_func(key_t key, size_t size, Func func, const void* addr = 0);

    /*!Returns the size of the shared memory segment. Never throws.*/
    size_t get_size()  const;

    /*!Returns shared memory segment's base address for this process.
       Never throws.*/
    void* get_base()  const;

    /*!Unmaps shared memory from process' address space. Never throws.*/
    void close();

    /*!Unmaps shared memory from process' address space . Never throws.
       It also executes the functor "func" atomically.
       Functor has must have the following signature:
       void operator()(const segment_info_t * info, bool last) const

       "info" is an initialized segment info structure, and "last"
       indicates if this unmapping is the last unmapping so that
       there will be no no other processes attached to the segment.
       The functor must not throw.*/
    template <class Func>
    void close_with_func(Func func);

private:

    //enum { Alignment  = boost::alignment_of<boost::detail::max_align>::value  };
    enum
    {
        Alignment = 8
    };

public:
    enum
    {
        segment_info_size = ct_rounded_size<sizeof(segment_info_t), Alignment>::value
    };

private:
    /*!No-op creation functor for xxx_with_func() functions*/
    struct null_construct_func_t
    {
        bool operator()(const segment_info_t* info, bool created) const
        {
            return true;
        }
    };

    /*!No-op creation functor for close_with_func() functions*/
    struct null_destroy_func_t
    {
        void operator()(const segment_info_t* info, bool last) const
        {
        }
    };

    template <class Func>
    bool priv_create(key_t key, size_t size, bool createonly, const void* addr, Func func);

    template <class Func>
    bool priv_open(key_t key, size_t size, const void* addr, Func f);

    template <class Func>
    void priv_close_with_func(Func func);

    void priv_close_handles();

    int                     _shm_handle;
    segment_info_t* _info_p;
};

inline bool SharedMemory::create(const char* name, size_t size, const void* addr)
{
    key_t key = ftok(name, 'p');
    return priv_create(key, size, true, addr, null_construct_func_t());
}

inline bool SharedMemory::create(key_t key, size_t size, const void* addr)
{
    return priv_create(key, size, true, addr, null_construct_func_t());
}

template <class Func>
inline bool SharedMemory::create_with_func(const char* name, size_t size, Func func, const void* addr)
{
    key_t key = ftok(name, 'p');
    return priv_create(key, size, true, addr, func);
}

template <class Func>
inline bool SharedMemory::create_with_func(key_t key, size_t size, Func func, const void* addr)
{
    return priv_create(key, size, true, addr, func);
}


inline bool SharedMemory::open(const char* name, size_t size, const void* addr)
{
    key_t key = ftok(name, 'p');
    return priv_open(key, size, addr, null_construct_func_t());
}

inline bool SharedMemory::open(key_t key , size_t size, const void* addr)
{
    return priv_open(key, size, addr, null_construct_func_t());
}

template <class Func>
bool SharedMemory::open_with_func(const char* name,size_t size, Func func, const void* addr)
{
    key_t key = ftok(name, 'p');
    return priv_open(key,size,  addr, func);
}

template <class Func>
bool SharedMemory::open_with_func(key_t key,size_t size, Func func, const void* addr)
{
    return priv_open(key,size,  addr, func);
}

inline bool SharedMemory::open_or_create(const char* name, size_t size, const void* addr)
{
    key_t key = ftok(name, 'p');
    return priv_create(key, size, false, addr, null_construct_func_t());
}

inline bool SharedMemory::open_or_create(key_t key, size_t size, const void* addr)
{
    return priv_create(key, size, false, addr, null_construct_func_t());
}


template <class Func>
inline bool SharedMemory::open_or_create_with_func(const char* name, size_t size, Func func, const void* addr)
{
    key_t key = ftok(key, 'p');
    return priv_create(key, size, false, addr, func);
}

template <class Func>
inline bool SharedMemory::open_or_create_with_func( key_t key, size_t size, Func func, const void* addr)
{
    return priv_create(key, size, false, addr, func);
}

inline void SharedMemory::close()
{
    this->priv_close_with_func(null_destroy_func_t());
}

template <class Func>
inline void SharedMemory::close_with_func(Func func)
{
    this->priv_close_with_func(func);
}

inline void* SharedMemory::segment_info_t::get_user_ptr() const
{
    return (char*)(this) + segment_info_size;
}

inline std::size_t SharedMemory::segment_info_t::get_user_size() const
{
    return this->real_size - segment_info_size;
}

inline SharedMemory::~SharedMemory()
{
    this->close();
}

inline size_t SharedMemory::get_size()  const
{
    return _info_p->get_user_size();
}

inline void* SharedMemory::get_base()  const
{
    return _info_p->get_user_ptr();
}


inline SharedMemory::SharedMemory()
        : _shm_handle(-1)
{
}

template <class Func>
inline bool SharedMemory::priv_create(key_t key, size_t size, bool createonly, const void* addr, Func func)
{
    if (_shm_handle != -1)
        return false;
    bool created = false;

    //Create new shared memory
    int oflag = IPC_CREAT | IPC_EXCL | 0666;
    size += segment_info_size;
    _shm_handle = shmget(key, size, oflag);
    if (_shm_handle == -1)
    {
        created = false;
        //Try to open the shared memory
        if ((!createonly) && (errno == EEXIST))
        {
            oflag = 0666;
            _shm_handle = shmget(key, size, oflag);
        }
        //If we can open the shared memory return error
        if (_shm_handle == -1)
        {
            this->priv_close_handles();
            return false;
        }
    }
    else
    {
        created = true;
    }


    //Map the segment_info_t structure to obtain the real size of the segment
    _info_p = reinterpret_cast<segment_info_t*>(shmat(_shm_handle, (void*)addr, 0));
//    if (int(_info_p) == -1)
    if ((void *)_info_p == (void *)-1)
	{
        this->priv_close_handles();
        //Don't unlink since we have just failed to open
        return false;
    }

    //Initialize data if segment was created
    if (created)
    {
        _info_p->real_size = size;
        _info_p->ref_count = 0;
    }
    else
    {
        //Obtain real size
        size = _info_p->real_size;
    }

    //Execute atomic function
    if (!func(const_cast<const segment_info_t*>(_info_p), created))
    {
        this->priv_close_handles();

        return false;
    }
    ++_info_p->ref_count;
    return true;
}

template <class Func>
inline bool SharedMemory::priv_open(key_t key, size_t size, const void* addr, Func func)
{
    if (_shm_handle != -1)
        return false;
    bool created = false;

    //Create new shared memory
    int oflag = 0666;


    size += segment_info_size;

    _shm_handle = shmget(key, size, oflag);
    if (_shm_handle == -1)
    {
        created = false;

        this->priv_close_handles();
        return false;
    }
    else
    {
        created = true;
    }

    //Map the segment_info_t structure to obtain the real size of the segment
    _info_p = reinterpret_cast<segment_info_t*>(shmat(_shm_handle, (void*)addr, 0));
//    if (int(_info_p) == -1)
  	if((void *)_info_p == (void *)-1)
	{
        this->priv_close_handles();
        //Don't unlink since we have just failed to open
        return false;
    }

    //Initialize data if segment was created
    if (created)
    {
        _info_p->real_size = size;
        _info_p->ref_count = 0;
    }
    else
    {
        //Obtain real size
        size = _info_p->real_size;
    }

    //Execute atomic function
    if (!func(const_cast<const segment_info_t*>(_info_p), created))
    {
        this->priv_close_handles();

        return false;
    }
    ++_info_p->ref_count;
    return true;
}

template <class Func>
inline void SharedMemory::priv_close_with_func(Func func)
{

}

inline void SharedMemory::priv_close_handles()
{

}





#endif
