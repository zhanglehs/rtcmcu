#include <new>
#include <iostream>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "shared.h"

using namespace std;

void* Shared::operator new(size_t blocksize, string pathname, int proj_id )
{
       Shared *rtn;
	int shmsize, shmid;
       key_t key;
       bool is_new;   //if the shared memory is created or attached
	
       key =  ftok(pathname.c_str(), proj_id);
	shmsize = blocksize;

       rtn = (Shared*)get_shm(key, shmsize, 0666, shmid, is_new);
//       if(reinterpret_cast<int>(rtn) == -1)
		if(rtn == NULL)
       {
	      	cerr << "Shared::new() shared memory get (shmget) failed ,error is "<<strerror(errno)<<endl;
	       throw bad_alloc();
        }

	rtn->shmid = shmid;
	rtn->key = key;
       rtn->new_or_attach =  is_new;
    
	return rtn;

}

void* Shared::operator new(size_t blocksize,  key_t key )
{
       Shared *rtn;
	int shmsize, shmid;
	bool is_new;
    
	shmsize = blocksize;

       rtn = (Shared*)get_shm(key, shmsize, 0666, shmid, is_new);
//       if(reinterpret_cast<int>(rtn) == -1)
		if(rtn == NULL)
       {
	      	cerr << "Shared::new() shared memory get (shmget) failed ,error is "<<strerror(errno)<<endl;
	       throw bad_alloc();
        }

	rtn->shmid = shmid;
	rtn->key = key;
       rtn->new_or_attach =  is_new;
       
	return rtn;

}


void Shared::operator delete(void *p)
{
       shmdt( p );
}



void * Shared::get_shm(int key, int shmsize, int shmflag, int& shmid, bool& is_new)
{
	char *p;
	
	if((shmid = shmget(key, shmsize, IPC_CREAT | IPC_EXCL | shmflag)) < 0)	
	{
		if (errno != EEXIST)
		{
                     cerr<<"getshm failed,error is "<<strerror(errno)<<endl;
			return NULL;
		}
		if((shmid = shmget(key, shmsize , shmflag)) < 0)				
		{
			return NULL;
		}
              is_new = false;
	}
       else
       {
            is_new = true;
       }
       
	if((p=(char *)shmat(shmid, 0, 0)) < 0 )
	{
		cerr<<"attache_shm failed ,error is "<<strerror(errno)<<endl;
		return NULL;
	}

	return p;

}

void * Shared::find_shm(int key, int shmsize, int shmflag, int& shmid)
{
	char *p;
	
	if((shmid = shmget(key,shmsize,shmflag& (~IPC_CREAT)) )< 0)	
	{
		if (errno != EEXIST)
		{
		       cerr<<"shm with key not exist,error is "<<strerror(errno)<<endl;
			return NULL;
		}
	}

	if((p=(char *)shmat(shmid, 0, 0)) < 0 )
	{
	       cerr<<"shm attach failed,error is "<<strerror(errno)<<endl;
		return NULL;
	}

	return p;
}


int Shared::attach_shm(int shmid,char **p)
{
	if((*p=(char *)shmat(shmid, 0, 0)) < 0 )
	{
		cerr<<"atach shm failed,error is "<<strerror(errno)<<endl;
		return -1;
	}
	return 0;
}

int Shared::detach_shm(char *p)
{
	if(shmdt(p)<0)
	{
		cerr<<"detach shm failed,error is "<<strerror(errno)<<endl;
		return -1;
	}
	return 0;
}

int Shared::remove_shm(int shmid)
{
	if(shmctl(shmid,IPC_RMID,0)<0)
	{
		cerr<<"remove shm failed,error is "<<strerror(errno)<<endl;
		return -1;
	}
	
	return 0;
}

void Shared::reset_shm(char * p,int iSize)
{
	memset(p, 0, iSize);
}



