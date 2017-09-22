#ifndef _BASE_NET_IS_PRIMITIVE_H_
#define _BASE_NET_IS_PRIMITIVE_H_

#define BOOL_TRAIT_SPEC(trait,sp,C) \
template<> struct trait< sp > \
{ \
 enum{    value=C};\
}; \

template<class T>
struct is_primitive
{
    enum { value = false };
};

BOOL_TRAIT_SPEC(is_primitive,long,true)
BOOL_TRAIT_SPEC(is_primitive,unsigned long,true)
BOOL_TRAIT_SPEC(is_primitive,int,true)
BOOL_TRAIT_SPEC(is_primitive,unsigned int,true)
BOOL_TRAIT_SPEC(is_primitive,short,true)
BOOL_TRAIT_SPEC(is_primitive,unsigned short,true)
BOOL_TRAIT_SPEC(is_primitive,char,true)
BOOL_TRAIT_SPEC(is_primitive,unsigned char,true)


#endif

