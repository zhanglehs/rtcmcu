#ifndef _NET_BASE_BOUTOARCHIVE_H_
#define _NET_BASE_BOUTOARCHIVE_H_

#include <iostream>
#include "bytebuffer.hpp"
#include "is_primitive.hpp"
#include "split_member.hpp"
#include <assert.h>

class BinOArchive
{
public:
    ByteBuffer& _buf;
public:
    BinOArchive(ByteBuffer& buf)
        :_buf(buf)
    {
    }    
    
    typedef bool_<false> is_loading;
    typedef bool_<true> is_saving;

    
    ByteBuffer& rdbuf(){ return _buf;}
    
    template<class T>
    BinOArchive& operator&(T& t)
    {
        return *this<<t;
    }

    BinOArchive& operator &(char* t)
    {
        return *this<<t;
    }

    BinOArchive& operator &(unsigned char* t)
    {
        return *this<<t;
    }
    
    BinOArchive& operator &(const char* t)
    {
        return *this<<t;
    }
    
    template <class T>
    BinOArchive & operator <<(T& t)
    {
        return save(t);
    }

    BinOArchive& operator <<(const char* str)
    {
	save(str);
        return *this;
    }

    BinOArchive& operator <<(unsigned char* str)
    {
	save((const char*)(str));
        return *this;
    }

    BinOArchive& operator <<(const unsigned char* str)
    {
	save((const char*)(str));
        return *this;
    }
    
    BinOArchive& operator <<(char* str)
    {
        return *this<<static_cast<const char*>(str);
    }
    
    template <class T>
    BinOArchive & save(T& t)
    {
        t.serialize(*this);
        return *this;
    }
    
    BinOArchive& save(const char* str);  
    
};

template<>
inline
BinOArchive& BinOArchive::save (char& c)
{
     _buf.put(c);

     return *this;
}

template<>
inline
BinOArchive& BinOArchive::save (unsigned char& c)
{
     _buf.put((char)c);

     return *this;
}

template<>
inline
BinOArchive& BinOArchive::save (short& n)
{
     _buf.putShort(n);

     return *this;
}

template<>
inline
BinOArchive& BinOArchive::save (unsigned short& n)
{
     _buf.putShort((unsigned short)n);

     return *this;
}

template<>
inline
BinOArchive& BinOArchive::save (int& n)
{
     _buf.putInt(n);

     return *this;
}

template<>
inline
BinOArchive& BinOArchive::save (unsigned int& n)
{
     _buf.putInt((unsigned int)n);

     return *this;
}
/*
template<>
inline
BinOArchive& BinOArchive::save (long& n)
{
     _buf.putInt(n);

     return *this;
}

template<>
inline
BinOArchive& BinOArchive::save (unsigned long& n)
{
     _buf.putInt((unsigned int)n);

    return *this;
}
*/
template<>
inline
BinOArchive& BinOArchive::save(int64_t& n)
{
     _buf.putLong((int64_t)n);

    return *this;
}

template<>
inline
BinOArchive& BinOArchive::save(uint64_t& n)
{
     _buf.putLong((uint64_t)n);

    return *this;
}

template<>
inline
BinOArchive& BinOArchive::save (const std::string& str)
{
    int length = str.length();
    *this<<length;

    if(length > 0)
    {
        _buf.put(str);
    }

    return *this;
}

template<>
inline
BinOArchive& BinOArchive::save (std::string& str)
{
	return save<const std::string>(str);
}


inline
BinOArchive& BinOArchive::save (const char *str)
{
    int  length = 0;
    if(str)
        length = strlen(str);

    *this<<length;
    if(length > 0)
    {
        _buf.put(str, length, length);
    }

    return *this;
}

template<>
inline
BinOArchive& BinOArchive::save (ByteBuffer& bb)
{
    int pos = bb.position();
    int lim = bb.limit();
    assert(pos <= lim);
    int rem = (pos <= lim ? lim - pos : 0);

    *this<<rem;
    if(rem > 0)
    {
        _buf.put(bb.address() + pos, rem, rem);
    }

    return *this;
}

#endif

