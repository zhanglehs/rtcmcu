#ifndef _NET_BASE_SPLIT_MEMBER_HPP_
#define _NET_BASE_SPLIT_MEMBER_HPP_

#include "eval_if.hpp"
#include "identity.hpp"


template<class Archive, class T>
struct member_saver 
{
    static void invoke(Archive & ar, const T & t)
    {
         const_cast<T &>(t).save(ar);
    }
};

template<class Archive, class T>
struct member_loader 
{
    static void invoke(Archive & ar, T & t)
    {
         const_cast<T &>(t).load(ar);
    }
};


template<class Archive, class T>
inline void split_member(Archive & ar, T & t)
{
    typedef typename eval_if<typename Archive::is_saving,
        identity<member_saver<Archive, T> >, 
        identity<member_loader<Archive, T> >
    >::type typex;
    typex::invoke(ar, t);
}


// split member function serialize funcition into save/load
#define ARCHIVE_SPLIT_MEMBER()                       \
template<class Archive>                                          \
void serialize(Archive &ar)                                           \
{                                                               \
    split_member(ar, *this); \
}                                                                \


#endif 

