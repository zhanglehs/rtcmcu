#ifndef _NET_BASE_BINIARCHIVE_H_
#define _NET_BASE_BINIARCHIVE_H_

#include <assert.h>
#include "bytebuffer.hpp"
#include "split_member.hpp"

class BinIArchive
{
public:
    BinIArchive(ByteBuffer& buf)
        :_buf(buf)
    {
    }
    
    ByteBuffer& rdbuf(){ return _buf;}
    ByteBuffer& _buf;

    typedef bool_<true> is_loading;
    typedef bool_<false> is_saving;

    
    template <class T>
    BinIArchive & operator &(T& t)
    {
        *this>>t;
        return *this;
    }


    BinIArchive& operator &(char* t)
    {
        return *this>>t;
    }
    
    BinIArchive& operator &(unsigned char* t)
    {
        return *this>>t;
    }
    
    BinIArchive& operator &(const char* t)
    {
        return *this>>t;
    }
    
    template <class T>
    BinIArchive & operator >>(T& t)
    {
        return load(t);
    }
    

    BinIArchive& operator >>(char* str)
    {
        return load(str);  
    }

    BinIArchive& operator >>(unsigned char* str)
    {
        return load((char*)str);  
    }
    
    BinIArchive& operator >>(const char* str)
    {
        return *this>>const_cast<char*>(str);
    }
    

    template <class T>
    BinIArchive& load(T& t)
    {
        t.serialize(*this);
        return *this;
    }

    BinIArchive& load(char* str);    
};

template<>
inline
BinIArchive& BinIArchive::load(char& c)
{
    c=_buf.get();
    return *this;
}

template<>
inline
BinIArchive& BinIArchive::load(unsigned char& c)
{
    c=(unsigned char)_buf.get();
    return *this;
}

template<>
inline
BinIArchive& BinIArchive::load(short& n)
{
    n=_buf.getShort();
    return *this;
}

template<>
inline
BinIArchive& BinIArchive::load(unsigned short& n)
{
    n=(unsigned short)_buf.getShort();
    return *this;
}

template<>
inline
BinIArchive& BinIArchive::load(int& n)
{
    n=_buf.getInt();
    return *this;
}

template<>
inline
BinIArchive& BinIArchive::load(unsigned int& n)
{
    n=(unsigned)_buf.getInt();
    return *this;
}
/*
template<>
inline
BinIArchive& BinIArchive::load(long& n)
{
    n=_buf.getInt();
    return *this;
}

template<>
inline
BinIArchive& BinIArchive::load(unsigned long& n)
{
    n=(unsigned)_buf.getInt();
    return *this;
}
*/
template<>
inline
BinIArchive& BinIArchive::load(int64_t& n)
{
	n = (int64_t)_buf.getLong();
	return *this;
}

template<>
inline
BinIArchive& BinIArchive::load(uint64_t& n)
{
	n = (uint64_t)_buf.getLong();
	return *this;
}

inline
BinIArchive& BinIArchive::load(char* str)
{
    int length;
    *this>>length;
    _buf.get(str, length, length);
    str[length] = '\0';

    return *this;
}

template<>
inline
BinIArchive& BinIArchive::load(std::string& str)
{
    int length;
   *this>>length;
    _buf.get(str, length);

    return *this;
}


template<>
inline
BinIArchive& BinIArchive::load (ByteBuffer& bb)
{
    int length;
    int pos = bb.position();
    int lim = bb.limit();
    assert(pos <= lim);
    int rem = (pos <= lim ? lim - pos : 0);

    *this>>length;
    if(rem > 0)
    {
        _buf.get(bb.address() + pos, rem, length);
    }

    if (length > 0)
    {
        bb.position(pos + length);
    }

    bb.flip();
    
    return *this;
}

#endif

