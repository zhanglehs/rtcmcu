#include "bits.hpp"
#include "byteorder.hpp"

ByteOrder::ByteOrder(string name)
{
    this->name = name;
}


ByteOrder ByteOrder::nativeOrder()
{
    return Bits::byteOrder();
}


ByteOrder ByteOrder::big_endian("BIG_ENDIAN");
ByteOrder ByteOrder::little_endian("LITTLE_ENDIAN");
