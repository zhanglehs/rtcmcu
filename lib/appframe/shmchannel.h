/**
 * \file
 * \brief Shared 基于共享内存的Array
 * 
 * \note this class template is not thread-safe or multi-process safe
 */

#ifndef _SHM_CHANNEL_H_
#define _SHM_CHANNEL_H_

/*
 this class template is not thread-safe or multi-process safe
*/
#include <sstream>
#include <string>
#include "shared.h"

using namespace std;

template<class Element, size_t ssize> 
class ShmArrayChannel: public Shared
{
protected:
  size_t head;
  size_t tail;
  size_t start;
  size_t finish;

  Element elements[ssize];
  
public:
	
  ShmArrayChannel()
  {
  	if(is_new())
  	{

	   head = 0;
	   tail = 0;
	   start = 0;
	   finish = ssize;
  	}

  }
  
  ~ShmArrayChannel()
  {
  }

  size_t capacity() const
  {
       return finish - start;
  }
  
  size_t  size() const
  {
	  return (capacity() - head + tail) % capacity();
  }

  bool full() 
  {
     	if (tail >= head)
	{		
           if(((finish - tail) == 1) && (head == start))
           {
              return true; //channel full
	    }
			
	}
	else
	{
		if ((head - tail) ==1)
		{
			return true;  //channel full
		}	
	}

	return false;
  }
  
  bool empty()
  {
      if(head == tail)
      	{
		return true; //channel empty
      	}

	return false;
  }
  
  Element* begin()
  {
      if(head == tail)
      {
		return NULL; //channel empty
      }
 
	return &elements[head];
  }
  int push_back(const Element& element) 
  {
	if (tail >= head)
	{		
           if(((finish - tail) == 1) && (head == start))
           {
              return -1; //channel full
	    }
			
	}
	else
	{
		if ((head - tail) ==1)
		{
			return -1;  //channel full
		}	
	}
 
      elements[tail] = element;
      //memcpy(&elements[tail],&element, 60*1024);
      if(++tail == finish) tail = start;	  


	return 0;
}

	inline 
	int push(const Element& element)
	{
		  return push_back(element);
	}
	


   int pop_front()  
   {
      if(head == tail)
      	{
		return -1; //channel empty
      	}
	  
       if(++this->head == finish)
	{
	   	head = start;
       }

	return 0;
   }

   int pop_front(Element& element) 
   {
      if(head == tail)
      	{
		return -1; //channel empty
      	}

	element =  elements[head];  
       if(++this->head == finish)
	{
	   	head = start;
       }

	return 0;
   }
   	inline
	int pop(Element& element) 
	{
		return pop_front(element);
	}

  string  dump()
  {
	stringstream outstream;
	outstream<<"head:"<<head<<"tail:"<<tail<<"start:"<<start<<"finish"<<finish;

	return outstream.str();
  }
};

#endif
