#ifndef _NET_BASE_IDENTITY_HPP_
#define _NET_BASE_IDENTITY_HPP_

template<typename T>
struct identity
{
    typedef T type;
};

template<typename T>
struct make_identity
{
    typedef identity<T> type;
};


#endif 

