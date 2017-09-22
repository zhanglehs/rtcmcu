#ifndef _BASE_BUFFER_H_
#define _BASE_BUFFER_H_

#include "ioexception.hpp"


class Buffer
{
    // Invariants: mark <= position <= limit <= capacity
private:
    int _mark;// = -1;
    int _position;// = 0;
    int _limit;
    int _capacity;

    // Creates a new buffer with the given mark, position, limit, and capacity,
    // after checking invariants.
public:

    Buffer(int mark, int pos, int lim, int cap)
    {
        // package-private
        if (cap < 0)
            throw  IllegalArgumentException();
        this->_capacity = cap;
        limit(lim);
        position(pos);
        if (mark > 0)
        {
            if (mark > pos)
                throw  IllegalArgumentException();
            this->_mark = mark;
        }
    }

    /**
     * Returns this buffer's capacity. </p>
     *
     * @return  The capacity of this buffer
     */
    int capacity()
    {
        return _capacity;
    }

    /**
     * Returns this buffer's position. </p>
     *
     * @return  The position of this buffer
     */
    int position() const
    {
        return _position;
    }

    /**
     * Sets this buffer's position.  If the mark is defined and larger than the
     * new position then it is discarded. </p>
     *
     * @param  newPosition
     *  	   The new position value; must be non-negative
     *  	   and no larger than the current limit
     *
     * @return  This buffer
     *
     * @throws  IllegalArgumentException
     *  		If the preconditions on <tt>newPosition</tt> do not hold
     */
    Buffer& position(int newPosition)
    {
        if ((newPosition > _limit) || (newPosition < 0))
            throw  IllegalArgumentException();
        _position = newPosition;
        if (_mark > _position)
            _mark = -1;
        return *this;
    }

    /**
     * Returns this buffer's limit. </p>
     *
     * @return  The limit of this buffer
     */
    int limit() const
    {
        return _limit;
    }

    /**
     * Sets this buffer's limit.  If the position is larger than the new limit
     * then it is set to the new limit.  If the mark is defined and larger than
     * the new limit then it is discarded. </p>
     *
     * @param  newLimit
     *  	   The new limit value; must be non-negative
     *  	   and no larger than this buffer's capacity
     *
     * @return  This buffer
     *
     * @throws  IllegalArgumentException
     *  		If the preconditions on <tt>newLimit</tt> do not hold
     */
    Buffer& limit(int newLimit)
    {
        if ((newLimit > _capacity) || (newLimit < 0))
            throw  IllegalArgumentException();
        _limit = newLimit;
        if (_position > _limit)
            _position = _limit;
        if (_mark > _limit)
            _mark = -1;
        return *this;
    }

    /**
     * Sets this buffer's mark at its position. </p>
     *
     * @return  This buffer
     */
    Buffer& mark()
    {
        _mark = _position;
        return *this;
    }

    /**
     * Resets this buffer's position to the previously-marked position.
     *
     * <p> Invoking this method neither changes nor discards the mark's
     * value. </p>
     *
     * @return  This buffer
     *
     * @throws  InvalidMarkException
     *  		If the mark has not been set
     */
    Buffer& reset()
    {
        int m = _mark;
        if (m < 0)
            throw  InvalidMarkException();
        _position = m;
        return *this;
    }

    /**
     * Clears this buffer.  The position is set to zero, the limit is set to
     * the capacity, and the mark is discarded.
     *
     * <p> Invoke this method before using a sequence of channel-read or
     * <i>put</i> operations to fill this buffer.  For example:
     *
     * <blockquote><pre>
     * buf.clear(); 	// Prepare buffer for reading
     * in.read(buf);	// Read data</pre></blockquote>
     *
     * <p> This method does not actually erase the data in the buffer, but it
     * is named as if it did because it will most often be used in situations
     * in which that might as well be the case. </p>
     *
     * @return  This buffer
     */
    Buffer& clear()
    {
        _position = 0;
        _limit = _capacity;
        _mark = -1;
        return *this;
    }

    /**
     * Flips this buffer.  The limit is set to the current position and then
     * the position is set to zero.  If the mark is defined then it is
     * discarded.
     *
     * <p> After a sequence of channel-read or <i>put</i> operations, invoke
     * this method to prepare for a sequence of channel-write or relative
     * <i>get</i> operations.  For example:
     *
     * <blockquote><pre>
     * buf.put(magic);    // Prepend header
     * in.read(buf);	  // Read data into rest of buffer
     * buf.flip();  	  // Flip buffer
     * out.write(buf);    // Write header + data to channel</pre></blockquote>
     *
     * <p> This method is often used in conjunction with the {@link
     * java.nio.ByteBuffer#compact compact} method when transferring data from
     * one place to another.  </p>
     *
     * @return  This buffer
     */
    Buffer& flip()
    {
        _limit = _position;
        _position = 0;
        _mark = -1;
        return *this;
    }

    /**
     * Rewinds this buffer.  The position is set to zero and the mark is
     * discarded.
     *
     * <p> Invoke this method before a sequence of channel-write or <i>get</i>
     * operations, assuming that the limit has already been set
     * appropriately.  For example:
     *
     * <blockquote><pre>
     * out.write(buf);    // Write remaining data
     * buf.rewind();	  // Rewind buffer
     * buf.get(array);    // Copy data into array</pre></blockquote>
     *
     * @return  This buffer
     */
    Buffer& rewind()
    {
        _position = 0;
        _mark = -1;
        return *this;
    }

    /**
     * Returns the number of elements between the current position and the
     * limit. </p>
     *
     * @return  The number of elements remaining in this buffer
     */
    int remaining() const
    {
        return _limit - _position;
    }

    /**
     * Tells whether there are any elements between the current position and
     * the limit. </p>
     *
     * @return  <tt>true</tt> if, and only if, there is at least one element
     *  		remaining in this buffer
     */
    bool hasRemaining()
    {
        return _position < _limit;
    }

    /**
     * Tells whether or not this buffer is read-only. </p>
     *
     * @return  <tt>true</tt> if, and only if, this buffer is read-only
     */
    //bool isReadOnly();


    // -- Package-private methods for bounds checking, etc. --

    /**
     * Checks the current position against the limit, throwing a {@link
     * BufferUnderflowException} if it is not smaller than the limit, and then
     * increments the position. </p>
     *
     * @return  The current position value, before it is incremented
     */
    int nextGetIndex()
    {
        // package-private
        if (_position >= _limit)
            throw  BufferUnderflowException();
        return _position++;
    }

    int nextGetIndex(int nb)
    {
        // package-private
        if (_position + nb > _limit)
            throw  BufferUnderflowException();
        int p = _position;
        _position += nb;
        return p;
    }

    /**
     * Checks the current position against the limit, throwing a {@link
     * BufferOverflowException} if it is not smaller than the limit, and then
     * increments the position. </p>
     *
     * @return  The current position value, before it is incremented
     */
    int nextPutIndex()
    {
        // package-private
        if (_position >= _limit)
            throw  BufferOverflowException();
        return _position++;
    }

    int nextPutIndex(int nb)
    {
        // package-private
        if (_position + nb > _limit)
            throw  BufferOverflowException();
        int p = _position;
        _position += nb;
        return p;
    }

    /**
     * Checks the given index against the limit, throwing an {@link
     * IndexOutOfBoundsException} if it is not smaller than the limit
     * or is smaller than zero.
     */
    int checkIndex(int i) const
    {
        // package-private
        if ((i < 0) || (i >= _limit))
            throw  IndexOutOfBoundsException();
        return i;
    }

    int checkIndex(int i, int nb)
    {
        // package-private
        if ((i < 0) || (i + nb > _limit))
            throw  IndexOutOfBoundsException();
        return i;
    }

    int markValue()
    {
        // package-private
        return _mark;
    }

    static void checkBounds(int off, int len, int size)
    {
        // package-private
        if ((off | len | (off + len) | (size - (off + len))) < 0)
            throw  IndexOutOfBoundsException();
    }
};

#endif

