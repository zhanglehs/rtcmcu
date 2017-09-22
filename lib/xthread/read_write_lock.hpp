// -*-mode:c++; coding:gb18030-*-

#ifndef __RWLOCK_HPP__
#define __RWLOCK_HPP__


#include <pthread.h>
#include <assert.h>

#include "exceptions.hpp"
#include "noncopyable.hpp"


/**
 * ReadWriteLock - 对线程的读写锁的简单封装
 *
 * 与fast_lock的实现类似，并提供相同的接口，可以与Guard<>对象结合使用，
 * 如下：
 *
 * ReadWriteLock the_lock;
 * {
 * 	Guard<ReadWriteLock::ReadLock> guard(the_lock.read_lock());
 * 	// here holds the read lock
 * }
 * {
 * 	Guard<ReadWriteLock::WriteLock> guard(the_lock.write_lock());
 * 	// here holds the write lock
 * }
 *
 */

class ReadWriteLock : private NonCopyable {
	
public:
	inline
	ReadWriteLock () : _rdlock(*this)
			 , _wrlock(*this) {
		int ret = pthread_rwlock_init (&_rwlock, NULL);
		if (ret != 0) {
			throw Initialization_Exception();
		}
	}

	~ReadWriteLock () {
		int ret = pthread_rwlock_destroy (&_rwlock);
		if (ret != 0) {
			//assert(0);
		}
	}

	// 提供通常的读写锁接口

	inline
	void acquire_read () {
		int ret = pthread_rwlock_rdlock (&_rwlock);
		if (ret != 0) {
			throw Synchronization_Exception();
		}
	}

	inline
	void acquire_write () {
		int ret = pthread_rwlock_wrlock (&_rwlock);
		if (ret != 0) {
			throw Synchronization_Exception();
		}
	}

	inline
	bool try_acquire_read (unsigned long /*timeout*/ = 0) {
		int ret = pthread_rwlock_tryrdlock (&_rwlock);
		return ret == 0;
	}

	inline
	bool try_acquire_write (unsigned long /*timeout*/ = 0) {
		int ret = pthread_rwlock_trywrlock (&_rwlock);
		return ret == 0;
	}

	inline
	void release () {
		int ret = pthread_rwlock_unlock (&_rwlock);
		if (ret != 0) {
			throw Synchronization_Exception();
		}
	}

	
	// 提供分离的读写锁抽象，使其可以像互斥锁一样来使用，并能与
	// Guard一起使用。

	
	class ReadLock : private NonCopyable {
	private:
		ReadWriteLock &_lock;

	public:
		inline
		ReadLock (ReadWriteLock &lock) : _lock(lock) {}

		inline
		~ReadLock () {}
		
		inline
		void acquire () {
			_lock.acquire_read();
		}

		inline
		bool try_acquire (unsigned long timeout = 0) {
			return _lock.try_acquire_read(timeout);
		}

		inline
		void release () {
			_lock.release();
		}
		
	};

	class WriteLock : private NonCopyable {
	private:
		ReadWriteLock &_lock;
	public:
		WriteLock (ReadWriteLock &lock) : _lock(lock) {}

		~WriteLock () {}
		
		inline
		void acquire () {
			_lock.acquire_write();
		}

		inline
		bool try_acquire (unsigned long timeout = 0) {
			return _lock.try_acquire_write(timeout);
		}

		inline
		void release () {
			_lock.release();
		}
	};

	// 获取内部读锁抽象
	ReadLock &read_lock () {
		return _rdlock;
	}

	// 获取内部写锁抽象
	WriteLock &write_lock () {
		return _wrlock;
	}
	
private:
	pthread_rwlock_t _rwlock;

	// 分离的读写抽象
	ReadLock _rdlock;
	WriteLock _wrlock;

};

#endif	// __RWLOCK_HPP__
