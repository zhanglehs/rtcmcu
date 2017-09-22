#include "byteorder.hpp"
#include "bytebuffer.hpp"
#include "bits.hpp"


ByteOrder Bits::_byteOrder("LITTLE_ENDIAN");


short  Bits::getShortL(ByteBuffer& bb, int bi) 
{
    return makeShort(bb._get(bi + 1),
                     bb._get(bi + 0));
}

short  Bits::getShortB(ByteBuffer& bb, int bi) 
{
    return makeShort(bb._get(bi + 0),
                     bb._get(bi + 1));
}


short  Bits::getShort(ByteBuffer& bb, int bi, bool bigEndian) 
{
    return (bigEndian ? getShortB(bb, bi) : getShortL(bb, bi));
}

void  Bits::putShortL(ByteBuffer& bb, int bi, short x) 
{
    bb._put(bi + 0, short0(x));
    bb._put(bi + 1, short1(x));
}

void  Bits::putShortB(ByteBuffer& bb, int bi, short x) 
{
    bb._put(bi + 0, short1(x));
    bb._put(bi + 1, short0(x));
}

void  Bits::putShort(ByteBuffer& bb, int bi, short x, bool bigEndian) 
{
    if (bigEndian)
        putShortB(bb, bi, x);
    else
        putShortL(bb, bi, x);
}

// -- get/put int --

int  Bits::getIntL(ByteBuffer& bb, int bi) 
{
    return makeInt(bb._get(bi + 3),
                   bb._get(bi + 2),
                   bb._get(bi + 1),
                   bb._get(bi + 0));
}

int  Bits::getIntB(ByteBuffer& bb, int bi) 
{
    return makeInt(bb._get(bi + 0),
                   bb._get(bi + 1),
                   bb._get(bi + 2),
                   bb._get(bi + 3));
}

int  Bits::getInt(ByteBuffer& bb, int bi, bool bigEndian) 
{
    return (bigEndian ? getIntB(bb, bi) : getIntL(bb, bi));
}

void  Bits::putIntL(ByteBuffer& bb, int bi, int x) 
{
    bb._put(bi + 3, int3(x));
    bb._put(bi + 2, int2(x));
    bb._put(bi + 1, int1(x));
    bb._put(bi + 0, int0(x));
}

void  Bits::putIntB(ByteBuffer& bb, int bi, int x) {
    bb._put(bi + 0, int3(x));
    bb._put(bi + 1, int2(x));
    bb._put(bi + 2, int1(x));
    bb._put(bi + 3, int0(x));
}

void  Bits::putInt(ByteBuffer& bb, int bi, int x, bool bigEndian) 
{
    if (bigEndian)
        putIntB(bb, bi, x);
    else
        putIntL(bb, bi, x);
}

 // -- get/put long --

int64_t  Bits::getLongL(ByteBuffer& bb, int bi) 
{
return makeLong(bb._get(bi + 7),
		bb._get(bi + 6),
		bb._get(bi + 5),
		bb._get(bi + 4),
		bb._get(bi + 3),
		bb._get(bi + 2),
		bb._get(bi + 1),
		bb._get(bi + 0));
}

int64_t  Bits::getLongB(ByteBuffer& bb, int bi) 
{
return makeLong(bb._get(bi + 0),
		bb._get(bi + 1),
		bb._get(bi + 2),
		bb._get(bi + 3),
		bb._get(bi + 4),
		bb._get(bi + 5),
		bb._get(bi + 6),
		bb._get(bi + 7));
}


int64_t  Bits::getLong(ByteBuffer& bb, int bi, bool bigEndian) 
{
    return (bigEndian ? getLongB(bb, bi) : getLongL(bb, bi));
}


void  Bits::putLongL(ByteBuffer& bb, int bi, int64_t x) 
{
    bb._put(bi + 7, long7(x));
    bb._put(bi + 6, long6(x));
    bb._put(bi + 5, long5(x));
    bb._put(bi + 4, long4(x));
    bb._put(bi + 3, long3(x));
    bb._put(bi + 2, long2(x));
    bb._put(bi + 1, long1(x));
    bb._put(bi + 0, long0(x));
}


void  Bits::putLongB(ByteBuffer& bb, int bi, int64_t x)
{
    bb._put(bi + 0, long7(x));
    bb._put(bi + 1, long6(x));
    bb._put(bi + 2, long5(x));
    bb._put(bi + 3, long4(x));
    bb._put(bi + 4, long3(x));
    bb._put(bi + 5, long2(x));
    bb._put(bi + 6, long1(x));
    bb._put(bi + 7, long0(x));
}



void  Bits::putLong(ByteBuffer& bb, int bi, int64_t x, bool bigEndian) 
{
    if (bigEndian)
        putLongB(bb, bi, x);
    else
        putLongL(bb, bi, x);
}

// -- get/put float --

float  Bits::getFloatL(ByteBuffer& bb, int bi) {
//return Float.intBitsToFloat(getIntL(bb, bi));
    return 0;
}


float  Bits::getFloatB(ByteBuffer& bb, int bi) {
//return Float.intBitsToFloat(getIntB(bb, bi));
    return 0;
}



float  Bits::getFloat(ByteBuffer& bb, int bi, bool bigEndian) {
//return (bigEndian ? getFloatB(bb, bi) : getFloatL(bb, bi));
    return 0;
}



void  Bits::putFloatL(ByteBuffer& bb, int bi, float x) {
//putIntL(bb, bi, Float.floatToRawIntBits(x));
    return ;
}



void  Bits::putFloatB(ByteBuffer& bb, int bi, float x) {
//putIntB(bb, bi, Float.floatToRawIntBits(x));
    return ;
}



void  Bits::putFloat(ByteBuffer& bb, int bi, float x, bool bigEndian) {
    if (bigEndian)
        putFloatB(bb, bi, x);
    else
        putFloatL(bb, bi, x);
}

ByteOrder  Bits::byteOrder()
{
    unsigned int order = 0x01020304;
    char* p   = (char*)&order;
    switch (*p)
    {
    case 0x01:
        _byteOrder = ByteOrder::big_endian;     break;
    case 0x04:
        _byteOrder = ByteOrder::little_endian;  break;
    default:
        break;

    }
    return _byteOrder;

}


