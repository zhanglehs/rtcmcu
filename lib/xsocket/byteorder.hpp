#ifndef _BASE_BYTEORDER_H_
#define _BASE_BYTEORDER_H_

#include "bits.hpp"

class ByteOrder
{
public:
    string name;
public:
    ByteOrder(string name) ;

    static ByteOrder big_endian;
    static ByteOrder little_endian;

    static ByteOrder nativeOrder();
};

#endif

