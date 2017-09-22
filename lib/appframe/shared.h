/**
 * \file 
 * \brief SYSV 共享内存的包装
 */

#ifndef _SHARED_HPP_
#define _SHARED_HPP_ 

#include <stdlib.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string>

class Shared
{
	private:
	  int shmid;
         int key;
         bool new_or_attach; //true new  ,false attach
        
	protected:
	  Shared() {}

        
        bool is_new(){ return new_or_attach;}
        int get_shmid(){ return shmid;}
        static void * get_shm(int key,int shmsize,int shmflg, int& shmid, bool& is_new);

        static void * find_shm(int key,int shmsize,int shmflg, int& shmid);

        static int attach_shm(int shmid,char **p);

        static int detach_shm(char *p);

        static int remove_shm(int shmid);

        static void reset_shm(char * p,int shmsize);
	public:
	  void *operator new(size_t,int key) ;	
	  void* operator new(size_t blocksize, std::string pathname, int proj_id );
	  void operator delete(void *);
};

#endif
