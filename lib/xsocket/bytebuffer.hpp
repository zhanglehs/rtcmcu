#ifndef _BASE_BYTEBUFFER_H_
#define  _BASE_BYTEBUFFER_H_

#include <string>
#include <sstream>
#include <cstdio>
#include <new>
#include <iostream>
#include <iomanip>
#include <string>
#include "byteorder.hpp"
#include "bits.hpp"
#include "buffer.hpp"

using namespace std;

typedef char byte;
class ByteOrder;
class Bits;



class ByteBuffer : public Buffer
{
    // These fields are declared here rather than in Heap-X-Buffer in order to
    // reduce the number of virtual method invocations needed to access these
    // values, which is especially costly when coding small buffers.
    //
    char* hb;       // Non-null only for heap buffers
    int offset;

    bool bigEndian;
    bool nativeByteOrder;
    bool isDirectAllocate;

public:
    // Creates a new buffer with the given mark, position, limit, capacity,
    // backing array, and array offset
    //
    ByteBuffer();
    ByteBuffer(int mark, int pos, int lim, int cap, char* hb, int offset);

    ByteBuffer(char* hb, int lim, int cap);
    ByteBuffer(int cap);
    ~ByteBuffer();

    static   void* arraycopy(byte* src, int  srcPos, byte* dest, int destPos, int length);

    const char* address() const
    {
        return hb + offset;
    }
    char* address()
    {
        return hb + offset;
    }
    void dump(std::ostream& dumped_stream) const;
    char* array() ;
    /**
     * Returns the offset within this buffer's backing array of the first
     * element of the buffer&nbsp;&nbsp;<i>(optional operation)</i>.
     *
     * <p> If this buffer is backed by an array then buffer position <i>p</i>
     * corresponds to array index <i>p</i>&nbsp;+&nbsp;<tt>arrayOffset()</tt>.
     *
     * <p> Invoke the {@link #hasArray hasArray} method before invoking this
     * method in order to ensure that this buffer has an accessible backing
     * array.  </p>
     *
     * @return  The offset within this buffer's array
     *  		of the first element of the buffer
     *
     * @throws  ReadOnlyBufferException
     *  		If this buffer is backed by an array but is read-only
     *
     * @throws  UnsupportedOperationException
     *  		If this buffer is not backed by an accessible array
     */
    int arrayOffset() ;

    int hashCode() ;

    /**
     * Retrieves this buffer's byte order.
     *
     * <p> The byte order is used when reading or writing multibyte values, and
     * when creating buffers that are views of this byte buffer.  The order of
     * a newly-created byte buffer is always {@link ByteOrder#BIG_ENDIAN
     * BIG_ENDIAN}.  </p>
     *
     * @return  This buffer's byte order
     */
    ByteOrder order();
    /**
     * Modifies this buffer's byte order.  </p>
     *
     * @param  bo
     *  	   The new byte order,
     *  	   either {@link ByteOrder#BIG_ENDIAN BIG_ENDIAN}
     *  	   or {@link ByteOrder#LITTLE_ENDIAN LITTLE_ENDIAN}
     *
     * @return  This buffer
     */

    ByteBuffer& order(ByteOrder bo);
    ByteBuffer slice();
    ByteBuffer duplicate() ;
    ByteBuffer&  slice(ByteBuffer& bb) ;
    ByteBuffer&  duplicate(ByteBuffer& bb) ;

	ssize_t copy_to(ByteBuffer& bb) const;

    int ix(int i) const;

    //byte
    byte get() ;
    byte get(int i) const;
    ByteBuffer& get(byte* dst, int dstLength, int offset, int length) ;
    ByteBuffer& get(byte* dst, int dstLength, int length) ;
    ByteBuffer& get(string& dst, int offset, int length);
    ByteBuffer& get(string& dst, int length);
    ByteBuffer& getAsciiz(char* dst,int dstLength);

    ByteBuffer& put(byte x) ;
    ByteBuffer& put(int i, byte x) ;
    ByteBuffer& put(const byte* src, int srcLength, int offset, int length) ;
    ByteBuffer& put(const byte* src, int srcLength, int length) ;
    ByteBuffer& put(const string& src, int offset) ;
    ByteBuffer& put(const string& src) ;
    ByteBuffer& put(ByteBuffer& src);
    ByteBuffer& putAsciiz(const byte* src, int srcLength);

    ByteBuffer& compact() ;


    byte _get(int i) ;

    void _put(int i, byte b);



    // short

    short getShort() ;
    short getShort(int i) ;
    ByteBuffer& putShort(short x) ;
    ByteBuffer& putShort(int i, short x) ;

    // int
    int getInt() ;
    int getInt(int i) ;
    ByteBuffer& putInt(int x) ;
    ByteBuffer& putInt(int i, int x);

    //long (int64)
    int64_t getLong();
    int64_t getLong(int i);
    ByteBuffer& putLong(int64_t x);
    ByteBuffer& putLong(int i, int64_t x);
    
    // float
    float getFloat() ;
    float getFloat(int i) ;
    ByteBuffer& putFloat(float x);
    ByteBuffer& putFloat(int i, float x);

        
};


inline
ByteBuffer::ByteBuffer()
        :Buffer(-1,0,0,0)
{
}

inline
ByteBuffer::ByteBuffer(int mark, int pos, int lim, int cap,	char* hb, int offset)
        :Buffer(mark, pos, lim, cap)
{
    isDirectAllocate = false;
    this->hb = hb;
    this->offset = offset;

    bigEndian	= true;
    nativeByteOrder	= (Bits::byteOrder().name== ByteOrder::big_endian.name);
}

inline
ByteBuffer::ByteBuffer(char* hb, int lim, int cap)
        :Buffer(-1, 0, lim, cap)
{
    isDirectAllocate = false;
    this->hb = hb;
    this->offset = 0;

    bigEndian	= true;
    nativeByteOrder	= (Bits::byteOrder().name== ByteOrder::big_endian.name);
}

inline
ByteBuffer::ByteBuffer(int cap)
        :Buffer(-1, 0, cap, cap)
{
    this->hb = new char[cap];
    this->isDirectAllocate = true;
    this->offset = 0;

    bigEndian	= true;
    nativeByteOrder	= (Bits::byteOrder().name== ByteOrder::big_endian.name);
}

inline
ByteBuffer::~ByteBuffer()
{
    if(isDirectAllocate)
    {
        delete[] this->hb;
        this->hb = NULL;
    }

}

inline
void ByteBuffer::dump(std::ostream& dumped_stream) const
{
    const int BYTES_INPUT_PER_LINE = 16;

    unsigned char uchChar = '\0';
    char achTextVer[BYTES_INPUT_PER_LINE + 1];

    int i;

    dumped_stream.fill('0');

    int lines = limit()/ BYTES_INPUT_PER_LINE;
    for (i = 0; i < lines; i++)
    {
        int  j;

        for (j = 0 ; j < BYTES_INPUT_PER_LINE; j++)
        {
            uchChar = (unsigned char) get((i << 4) + j);
            dumped_stream<<setw(2)<<hex<<(int)uchChar<<" ";

            if (j == 7)
            {
                dumped_stream<<" ";
            }
            achTextVer[j] = isprint(uchChar) ? uchChar : '.';

        }

        achTextVer[j] = 0;
        dumped_stream<<" "<<achTextVer<<endl;

    }

    if ( limit() % BYTES_INPUT_PER_LINE)
    {
        for (i = 0 ; i <  limit() % BYTES_INPUT_PER_LINE; i++)
        {
            uchChar = (unsigned char) get(limit() - limit() % BYTES_INPUT_PER_LINE + i);
            dumped_stream<<setw(2)<<hex<<(int)uchChar<<" ";

            if (i == 7)
            {
                dumped_stream<<" ";
            }

            achTextVer[i] = isprint (uchChar) ? uchChar : '.';
        }

        for (i = limit() % BYTES_INPUT_PER_LINE; i < BYTES_INPUT_PER_LINE; i++)
        {
            dumped_stream<<"   ";
            if (i == 7)
            {
                dumped_stream<<" ";
            }
            achTextVer[i] = ' ';
        }

        achTextVer[i] = 0;
        dumped_stream<<" "<<achTextVer<<endl;
    }

    return ;
}

inline
void* ByteBuffer::arraycopy(byte* src,  int  srcPos,
                            byte* dest, int destPos,
                            int length)
{
    byte*  _src = src+srcPos;
    byte* _dest = dest+destPos;
    /**
    if (from > to) {
    while (count-- > 0) {
    *to++ = *from++; // copy forwards
    }
    } else {
    from = &from[count];
    to = &to[count];
    while (count-- > 0) {
    *--to = *--from; // copy backwards
    }
    }
    */
    return memmove(_dest, _src, length);

}
/**
 * Returns the byte array that backs this
 * buffer&nbsp;&nbsp;<i>(optional operation)</i>.
 *
 * <p> Modifications to this buffer's content will cause the returned
 * array's content to be modified, and vice versa.
 *
 * <p> Invoke the {@link #hasArray hasArray} method before invoking this
 * method in order to ensure that this buffer has an accessible backing
 * array.  </p>
 *
 * @return  The array that backs this buffer
 *
 * @throws  ReadOnlyBufferException
 *          If this buffer is backed by an array but is read-only
 *
 * @throws  UnsupportedOperationException
 *          If this buffer is not backed by an accessible array
 */

inline
char* ByteBuffer::array()
{
    if (hb == NULL)
        throw  UnsupportedOperationException();

    return hb;
}

/**
 * Returns the offset within this buffer's backing array of the first
 * element of the buffer&nbsp;&nbsp;<i>(optional operation)</i>.
 *
 * <p> If this buffer is backed by an array then buffer position <i>p</i>
 * corresponds to array index <i>p</i>&nbsp;+&nbsp;<tt>arrayOffset()</tt>.
 *
 * <p> Invoke the {@link #hasArray hasArray} method before invoking this
 * method in order to ensure that this buffer has an accessible backing
 * array.  </p>
 *
 * @return  The offset within this buffer's array
 *          of the first element of the buffer
 *
 * @throws  ReadOnlyBufferException
 *          If this buffer is backed by an array but is read-only
 *
 * @throws  UnsupportedOperationException
 *          If this buffer is not backed by an accessible array
 */

inline
int ByteBuffer::arrayOffset()
{
    if (hb == NULL)
        throw  UnsupportedOperationException();

    return offset;
}

inline
int ByteBuffer::hashCode()
{
    int h = 1;
    int p = position();
    for (int i = limit() - 1; i >= p; i--)
        h = 31 * h + (int)get(i);
    return h;
}




/**
 * Retrieves this buffer's byte order.
 *
 * <p> The byte order is used when reading or writing multibyte values, and
 * when creating buffers that are views of this byte buffer.  The order of
 * a newly-created byte buffer is always {@link ByteOrder#BIG_ENDIAN
 * BIG_ENDIAN}.  </p>
 *
 * @return  This buffer's byte order
 */

inline
ByteOrder ByteBuffer::order()
{
    return bigEndian ? ByteOrder::big_endian: ByteOrder::little_endian;
}

/**
 * Modifies this buffer's byte order.  </p>
 *
 * @param  bo
 *         The new byte order,
 *         either {@link ByteOrder#BIG_ENDIAN BIG_ENDIAN}
 *         or {@link ByteOrder#LITTLE_ENDIAN LITTLE_ENDIAN}
 *
 * @return  This buffer
 */

inline
ByteBuffer& ByteBuffer::order(ByteOrder bo)
{
    bigEndian = (bo.name == ByteOrder::big_endian.name);
    nativeByteOrder =
        (bigEndian == (Bits::byteOrder().name == ByteOrder::big_endian.name));
    return *this;
}



inline
ByteBuffer ByteBuffer::slice()
{
    ByteBuffer bb(-1,
                  0,
                  this->remaining(),
                  this->remaining(),
                  this->hb,
                  this->position() + offset);
    return bb;
}


inline
ByteBuffer ByteBuffer::duplicate()
{
    ByteBuffer bb(this->markValue(),
                   this->position(),
                   this->limit(),
                   this->capacity(),
                   this->hb,
                   offset);
    return bb;
}

inline
ssize_t ByteBuffer::copy_to(ByteBuffer& bb) const
{
	int pos = position();
	int lim = limit();

	int rem = (pos <= lim ? lim - pos : 0);
	
	if (rem == 0)
	{
		return 0;
	}

	int other_pos = bb.position();
	int other_lim = bb.limit();

	int other_rem = (other_pos <= other_lim ? other_lim - other_pos : 0);

	if (other_rem < rem)
	{
		return 0;
	}
	
	memcpy(bb.address() + other_pos, address() + pos, rem);

//	bb.position(other_pos + rem);
	try
	{
		//bb.limit(other_lim + rem);
		bb.limit(other_pos + rem);
	}
	catch(std::exception&)
	{
		return -1;
	}
	
	return static_cast<ssize_t>(rem);
}

inline
ByteBuffer& ByteBuffer::slice(ByteBuffer& bb)
{
    return *(new(&bb)ByteBuffer(
                 -1,
                 0,
                 this->remaining(),
                 this->remaining(),
                 this->hb,
                 this->position() + offset));
}


inline
ByteBuffer& ByteBuffer::duplicate(ByteBuffer& bb)
{
    return *(new(&bb)ByteBuffer(
                 this->markValue(),
                 this->position(),
                 this->limit(),
                 this->capacity(),
                 this->hb,
                 offset));
}


inline
int ByteBuffer::ix(int i) const
{
    return i + offset;
}


inline
char ByteBuffer::get()
{
    return hb[ix(nextGetIndex())];
}


inline
char ByteBuffer::get(int i) const
{
    return hb[ix(checkIndex(i))];
}


inline
ByteBuffer& ByteBuffer::get(char* dst, int dstLength,int offset, int length)
{
    checkBounds(offset, length, dstLength);
    if (length > remaining())
        throw  BufferUnderflowException();
    arraycopy(hb, ix(position()), dst, offset, length);
    position(position() + length);
    return *this;
}


inline
ByteBuffer& ByteBuffer::get(string& dst,int offset, int length)
{
    dst.resize(length);
    checkBounds(offset, length, length);
    if (length > remaining())
        throw  BufferUnderflowException();
    //arraycopy(hb, ix(position()), dst, offset, length);
    dst.assign(hb+ix(position()), length);
    position(position() + length);
    return *this;
}


inline
ByteBuffer& ByteBuffer::get(char* dst,int dstLength, int length)
{
    return get(dst, dstLength, 0, length);
}


inline
ByteBuffer& ByteBuffer::get(string& dst, int length)
{
    return get(dst, 0, length);
}


inline
ByteBuffer&  ByteBuffer::getAsciiz(char* dst,int dstLength)
{
    int i = 0;
    for(i = 0; i < dstLength; i++)
    {
        dst[i]= get();
        if(dst[i] == 0)
        {
            break;
        }
    }

    if((i == (dstLength-1)) && (dst[i] !=0))
        throw  IndexOutOfBoundsException();

    return * this;
}


inline
ByteBuffer& ByteBuffer::put(char x)
{
    hb[ix(nextPutIndex())] = x;
    return *this;
}

inline
ByteBuffer& ByteBuffer::put(int i, char x)
{
    hb[ix(checkIndex(i))] = x;
    return *this;
}

inline
ByteBuffer& ByteBuffer::put(const char* src, int srcLength,  int offset, int length)
{
    checkBounds(offset, length, srcLength);
    if (length > remaining())
        throw  BufferOverflowException();
    arraycopy(const_cast<char*>(src), offset, hb, ix(position()), length);
    position(position() + length);
    return *this;
}


inline
ByteBuffer& ByteBuffer::put(const char* src, int srcLength,   int length)
{
    return put(src,srcLength , 0, length);
}

inline
ByteBuffer& ByteBuffer::putAsciiz(const byte* src, int srcLength)
{
    int length = strlen(src);
    if(length == 0)
    {
        put(0);
    }
    else
    {
        put(src, srcLength, 0, length);
        put(0);
    }

    return * this;
}


inline
ByteBuffer& ByteBuffer::put(const string& src,  int offset)
{
    int length = src.length();
    checkBounds(offset, length, length);
    if (length > remaining())
        throw  BufferOverflowException();
    arraycopy(const_cast<char*>(src.data()), offset, hb, ix(position()), src.length());
    position(position() + length);
    return *this;
}

inline
ByteBuffer& ByteBuffer::put(const string& src)
{
    return put(src,  0);
}

inline
ByteBuffer&  ByteBuffer::put(ByteBuffer& src)
{
    if (&src == this)
        throw  IllegalArgumentException();
    ByteBuffer& sb = src;
    int n = sb.remaining();
    if (n > remaining())
        throw  BufferOverflowException();
    arraycopy(sb.hb, sb.ix(sb.position()),
              hb, ix(position()), n);
    sb.position(sb.position() + n);
    position(position() + n);

    return *this;
}

inline
ByteBuffer& ByteBuffer::compact()
{
    if( ix(position()) != ix(0))
    {
        arraycopy(hb, ix(position()), hb, ix(0), remaining());
    }
    position(remaining());
    limit(capacity());
    return *this;
}

inline
char ByteBuffer::_get(int i)
{
    return hb[i];
}

inline
void ByteBuffer::_put(int i, char b)
{
    hb[i] = b;
}

// short

inline
short ByteBuffer::getShort()
{
    return Bits::getShort(*this, ix(nextGetIndex(2)), bigEndian);
}

inline
short ByteBuffer::getShort(int i)
{
    return Bits::getShort(*this, ix(checkIndex(i, 2)), bigEndian);
}


inline
ByteBuffer& ByteBuffer::putShort(short x)
{
    Bits::putShort(*this, ix(nextPutIndex(2)), x, bigEndian);
    return *this;
}

inline
ByteBuffer& ByteBuffer::putShort(int i, short x)
{
    Bits::putShort(*this, ix(checkIndex(i, 2)), x, bigEndian);
    return *this;
}

// int


inline
int ByteBuffer::getInt()
{
    return Bits::getInt(*this, ix(nextGetIndex(4)), bigEndian);
}

inline
int ByteBuffer::getInt(int i)
{
    return Bits::getInt(*this, ix(checkIndex(i, 4)), bigEndian);
}


inline
ByteBuffer& ByteBuffer::putInt(int x)
{
    Bits::putInt(*this, ix(nextPutIndex(4)), x, bigEndian);
    return *this;
}

inline
ByteBuffer& ByteBuffer::putInt(int i, int x)
{
    Bits::putInt(*this, ix(checkIndex(i, 4)), x, bigEndian);
    return *this;
}

//long (int 64)
inline
int64_t ByteBuffer::getLong()
{
    return Bits::getLong(*this, ix(nextGetIndex(8)), bigEndian);
}

inline
int64_t ByteBuffer::getLong(int i)
{
    return Bits::getLong(*this, ix(checkIndex(i, 8)), bigEndian);
}


inline
ByteBuffer& ByteBuffer::putLong(int64_t x)
{
    Bits::putLong(*this, ix(nextPutIndex(8)), x, bigEndian);
    return *this;
}

inline
ByteBuffer& ByteBuffer::putLong(int i, int64_t x)
{
    Bits::putLong(*this, ix(checkIndex(i, 8)), x, bigEndian);
    return *this;
}

// float
inline
float ByteBuffer::getFloat()
{
    return Bits::getFloat(*this, ix(nextGetIndex(4)), bigEndian);
}

inline
float ByteBuffer::getFloat(int i)
{
    return Bits::getFloat(*this, ix(checkIndex(i, 4)), bigEndian);
}


inline
ByteBuffer& ByteBuffer::putFloat(float x)
{
    Bits::putFloat(*this, ix(nextPutIndex(4)), x, bigEndian);
    return *this;
}

inline
ByteBuffer& ByteBuffer::putFloat(int i, float x)
{
    Bits::putFloat(*this, ix(checkIndex(i, 4)), x, bigEndian);
    return *this;
}

static inline
std::ostream& operator<<(std::ostream& out, const ByteBuffer& byte_buffer)
{
	byte_buffer.dump(out);
	return out;
}

#endif

