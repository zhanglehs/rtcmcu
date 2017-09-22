#ifndef _BASE_BITS_H_
#define _BASE_BITS_H_

#include <stdint.h>
#include <string>

using namespace std;

typedef  char byte;

class ByteBuffer;
class ByteOrder;

class Bits
{
private:
    static ByteOrder _byteOrder;

public:
    Bits() ;


    // -- Swapping --

    static short swap(short x) ;

    static char swap(char x) ;

    static int swap(int x) ;

    // -- get/put short --

    static  short makeShort(byte b1, byte b0) ;

    static short getShortL(ByteBuffer& bb, int bi) ;



    static short getShortB(ByteBuffer& bb, int bi);


    static short getShort(ByteBuffer& bb, int bi, bool bigEndian);

    static char short1(short x) ;
    static char short0(short x) ;

    static void putShortL(ByteBuffer& bb, int bi, short x);


    static void putShortB(ByteBuffer& bb, int bi, short x) ;



    static void putShort(ByteBuffer& bb, int bi, short x, bool bigEndian) ;




    // -- get/put int --

    static  int makeInt(byte b3, byte b2, byte b1, byte b0) ;
    static int getIntL(ByteBuffer& bb, int bi) ;
    static int getIntB(ByteBuffer& bb, int bi);
    static int getInt(ByteBuffer& bb, int bi, bool bigEndian) ;

    static char int3(int x);
    static char int2(int x);
    static char int1(int x);
    static char int0(int x);

    static void putIntL(ByteBuffer& bb, int bi, int x) ;
    static void putIntB(ByteBuffer& bb, int bi, int x) ;
    static void putInt(ByteBuffer& bb, int bi, int x, bool bigEndian) ;

   //get/put long(int64)
    static int64_t makeLong(byte b7, byte b6, byte b5, byte b4,
			                byte b3, byte b2, byte b1, byte b0);
    static int64_t getLongL(ByteBuffer& bb, int bi) ;
    static int64_t getLongB(ByteBuffer& bb, int bi);
    static int64_t getLong(ByteBuffer& bb, int bi, bool bigEndian);
    
    static char  long7(int64_t x);
    static char  long6(int64_t x);
    static char  long5(int64_t x);
    static char  long4(int64_t x);
    static char  long3(int64_t x);
    static char  long2(int64_t x);
    static char  long1(int64_t x);
    static char  long0(int64_t x);

    static void putLongL(ByteBuffer& bb, int bi, int64_t x) ;
    static void putLongB(ByteBuffer& bb, int bi, int64_t x) ;
    static void putLong(ByteBuffer& bb, int bi, int64_t x, bool bigEndian) ;
    // -- get/put float --

    static float getFloatL(ByteBuffer& bb, int bi) ;


    static float getFloatB(ByteBuffer& bb, int bi) ;



    static float getFloat(ByteBuffer& bb, int bi, bool bigEndian) ;



    static void putFloatL(ByteBuffer& bb, int bi, float x) ;



    static void putFloatB(ByteBuffer& bb, int bi, float x) ;


    static void putFloat(ByteBuffer& bb, int bi, float x, bool bigEndian) ;

    static ByteOrder byteOrder() ;
};

inline
Bits::Bits()
{

}


// -- Swapping --

inline
short  Bits::swap(short x) {
    return (short)((x << 8) |
                   ((x >> 8) & 0xff));
}

inline
char  Bits::swap(char x) {
    return (char)((x << 8) |
                  ((x >> 8) & 0xff));
}

inline
int  Bits::swap(int x) {
    return (int)((swap((short)x) << 16) |
                 (swap((short)(x >> 16)) & 0xffff));
}


// -- get/put short --
inline
short  Bits::makeShort(byte b1, byte b0) {
    return (short)((b1 << 8) | (b0 & 0xff));
}

inline
int  Bits::makeInt(byte b3, byte b2, byte b1, byte b0) {
    return (int)((((b3 & 0xff) << 24) |
                  ((b2 & 0xff) << 16) |
                  ((b1 & 0xff) <<  8) |
                  ((b0 & 0xff) <<  0)));
}

inline
int64_t Bits::makeLong(byte b7, byte b6, byte b5, byte b4,
			 byte b3, byte b2, byte b1, byte b0)
{
return ((((int64_t)b7 & 0xff) << 56) |
	(((int64_t)b6 & 0xff) << 48) |
	(((int64_t)b5 & 0xff) << 40) |
	(((int64_t)b4 & 0xff) << 32) |
	(((int64_t)b3 & 0xff) << 24) |
	(((int64_t)b2 & 0xff) << 16) |
	(((int64_t)b1 & 0xff) <<  8) |
	(((int64_t)b0 & 0xff) <<  0));
}

inline
byte  Bits::short1(short x) { return (byte)(x >> 8); }

inline
byte  Bits::short0(short x) { return (byte)(x >> 0); }

inline
char  Bits::int3(int x) { return (byte)(x >> 24); }

inline
char  Bits::int2(int x) { return (byte)(x >> 16); }

inline
char  Bits::int1(int x) { return (byte)(x >>  8); }

inline
char  Bits::int0(int x) { return (byte)(x >>  0); }


inline
char  Bits::long7(int64_t x) { return (byte)(x >> 56); }
inline
char  Bits::long6(int64_t x) { return (byte)(x >> 48); }
inline
char  Bits::long5(int64_t x) { return (byte)(x >> 40); }
inline
char  Bits::long4(int64_t x) { return (byte)(x >> 32); }
inline
char  Bits::long3(int64_t x) { return (byte)(x >> 24); }
inline
char  Bits::long2(int64_t x) { return (byte)(x >> 16); }
inline
char  Bits::long1(int64_t x) { return (byte)(x >>  8); }
inline
char  Bits::long0(int64_t x) { return (byte)(x >>  0); }

#endif

