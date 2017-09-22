/**
 * \file
 * \brief 任意长度的标志位集合
 */
#ifndef _BIT_SET_H_
#define _BIT_SET_H_
#include <stdint.h>
/*
 * _trailing_zero_table[i] is the number of trailing zero bits in the binary
 * representation of i.
 */
static const char _trailing_zero_table[256]=
{
    -25, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};


template<int capacity>
class BitSet
{
    /*
     * BitSets are packed into arrays of "units."  Currently a unit is a uint32_t,
     * which consists of 32 bits, requiring 6 address bits.  The choice of unit
     * is determined purely by performance concerns.
     */
    static const  int ADDRESS_BITS_PER_UNIT = 5;
    static const  int BITS_PER_UNIT = 1 << ADDRESS_BITS_PER_UNIT;
    static const  int BIT_INDEX_MASK = BITS_PER_UNIT - 1;

    /* Used to shift left or right for a partial word mask */
    static const uint32_t WORD_MASK = 0xffffffff;


public:
    int math_min(int a, int b)
    {
        return a>=b?b:a;
    }
    /**
     * Given a bit index return unit index containing it.
     */
    static int unit_index(int bit_index)
    {
        return bit_index >> ADDRESS_BITS_PER_UNIT;
    }

    /**
     * Given a bit index, return a unit that masks that bit in its unit.
     */
    static uint32_t bit(int bit_index)
    {
        return 1 << (bit_index & BIT_INDEX_MASK);
    }

    /**
     * Set the field _units_in_use with the logical size in units of the bit
     * set.  WARNING:This function assumes that the number of units actually
     * in use is less than or equal to the current value of _units_in_use!
     */
    void recalculate_units_in_use()
    {
        // Traverse the bitset until a used unit is found
        int i;
        for (i = _units_in_use-1; i >= 0; i--)
        {
            if(_units[i] != 0)
            {
                break;
            }
        }

        _units_in_use = i+1; // The new logical size
    }


    /**
     * Returns a uint32_t that has all bits that are less significant
     * than the specified index set to 1. All other bits are 0.
     */
    static uint32_t bits_right_of(int x)
    {
        return (x==0 ? 0 : WORD_MASK >> (32-x));
    }

    /**
     * Returns a uint32_t that has all the bits that are more significant
     * than or equal to the specified index set to 1. All other bits are 0.
     */
    static uint32_t bits_left_of(int x)
    {
        return WORD_MASK << x;
    }

    /**
    * Returns the unit of this bitset at index j as if this bitset had an
    * infinite amount of storage.
    */
    uint32_t get_bits(int j)
    {
        return (j < _units_in_use) ? _units[j] : 0;
    }

    static int trailing_zero_cnt(uint32_t val)
    {
        // Loop unrolled for performance
        int byteVal = (int)val & 0xff;
        if (byteVal != 0)
            return _trailing_zero_table[byteVal];

        byteVal = (int)(val >> 8) & 0xff;
        if (byteVal != 0)
            return _trailing_zero_table[byteVal] + 8;

        byteVal = (int)(val >> 16) & 0xff;
        if (byteVal != 0)
            return _trailing_zero_table[byteVal] + 16;

        byteVal = (int)(val >> 24) & 0xff;
        return _trailing_zero_table[byteVal] + 24;
    }



    /**
     * bitLen(val) is the number of bits in val.
     */
    static int bitLen(int w)
    {
        // Binary search - decision tree (5 tests, rarely 6)
        return
            (w < 1<<15 ?
             (w < 1<<7 ?
              (w < 1<<3 ?
               (w < 1<<1 ? (w < 1<<0 ? (w<0 ? 32 : 0) : 1) : (w < 1<<2 ? 2 : 3)) :
                       (w < 1<<5 ? (w < 1<<4 ? 4 : 5) : (w < 1<<6 ? 6 : 7))) :
                      (w < 1<<11 ?
                       (w < 1<<9 ? (w < 1<<8 ? 8 : 9) : (w < 1<<10 ? 10 : 11)) :
                       (w < 1<<13 ? (w < 1<<12 ? 12 : 13) : (w < 1<<14 ? 14 : 15)))) :
                     (w < 1<<23 ?
                      (w < 1<<19 ?
                       (w < 1<<17 ? (w < 1<<16 ? 16 : 17) : (w < 1<<18 ? 18 : 19)) :
                       (w < 1<<21 ? (w < 1<<20 ? 20 : 21) : (w < 1<<22 ? 22 : 23))) :
                      (w < 1<<27 ?
                       (w < 1<<25 ? (w < 1<<24 ? 24 : 25) : (w < 1<<26 ? 26 : 27)) :
                       (w < 1<<29 ? (w < 1<<28 ? 28 : 29) : (w < 1<<30 ? 30 : 31)))));
    }

    /**
     * Returns the number of bits set in val.
     * For a derivation of this algorithm, see
     * "Algorithms and data structures with applications to
     *  graphics and geometry", by Jurg Nievergelt and Klaus Hinrichs,
     *  Prentice Hall, 1993.
     */
    static int bit_count(uint32_t val)
    {
        val -= (val & 0xaaaaaaaa) >> 1;
        val =  (val & 0x33333333) + ((val >> 2) & 0x33333333);
        val =  (val + (val >> 4)) & 0x0f0f0f0f;
        val += val >> 8;
        val += val >> 16;
        return ((int)(val)) & 0xff;
    }


public:


    /**
     * Creates a bit set whose initial size is large enough to explicitly
     * represent bits with indices in the range <code>0</code> through
     * <code>nbits-1</code>. All bits are initially <code>false</code>.
     *
     * @param     nbits   the initial size of the bit set.
     * @exception NegativeArraySizeException if the specified initial size
     *               is negative.
     */
    BitSet()
    :_capacity(capacity),
    _units_capacity(unit_index(capacity-1) + 1),
    _units_in_use(0)
    {
        memset(_units, 0, _units_capacity*sizeof(uint32_t));
    }

    int init()
    {
        _capacity = capacity;
        _units_capacity = unit_index(capacity-1) + 1;
        _units_in_use = 0;

        memset(_units, 0, _units_capacity*sizeof(uint32_t));

        return 0;
    }

    /**
     * Sets the bit at the specified index to the complement of its
     * current value.
     *
     * @param   bitIndex the index of the bit to flip.
     * @exception IndexOutOfBoundsException if the specified index is negative.
     * @since   1.4
     */
    int flip(int bitIndex)
    {
        if ((bitIndex < 0) || (bitIndex >=_capacity))
        {
            return -1;
        }


	int unitIndex = unit_index(bitIndex);
        int unitsRequired = unitIndex+1;

        if (_units_in_use < unitsRequired) 
        {
            _units[unitIndex] ^= bit(bitIndex);
            _units_in_use = unitsRequired;
        } 
        else 
        {
            _units[unitIndex] ^= bit(bitIndex);
            if (_units[_units_in_use-1] == 0)
            {
                recalculate_units_in_use();
            }
        }
        
        return 0;
    }

    /**
     * Sets each bit from the specified fromIndex(inclusive) to the
     * specified toIndex(exclusive) to the complement of its current
     * value.
     *
     * @param     fromIndex   index of the first bit to flip.
     * @param     toIndex index after the last bit to flip.
     * @exception IndexOutOfBoundsException if <tt>fromIndex</tt> is negative,
     *            or <tt>toIndex</tt> is negative, or <tt>fromIndex</tt> is
     *            larger than <tt>toIndex</tt>.
     * @since   1.4
     */
    int flip(int fromIndex, int toIndex)
    {
        
        if ((fromIndex < 0) || (fromIndex >=_capacity))
            return -1;
        if ((toIndex < 0)|| (toIndex >=_capacity))
            return -1;
        if (fromIndex > toIndex)
            return -1;
        

        // Increase capacity if necessary
        int endUnitIndex = unit_index(toIndex);
        int unitsRequired = endUnitIndex + 1;

        if (_units_in_use < unitsRequired) 
        {
            _units_in_use = unitsRequired;
        }

        int startUnitIndex = unit_index(fromIndex);
//        long bitMask = 0;
        int bitMask = 0;
        if (startUnitIndex == endUnitIndex) 
        {
            // Case 1: One word
            bitMask = (1L << (toIndex & BIT_INDEX_MASK)) -
                      (1L << (fromIndex & BIT_INDEX_MASK));
            _units[startUnitIndex] ^= bitMask;
            if (_units[_units_in_use-1] == 0)
            {
                recalculate_units_in_use();
            }
            
            return;
        }
        
        // Case 2: Multiple words
        // Handle first word
        bitMask = bits_left_of(fromIndex & BIT_INDEX_MASK);
        _units[startUnitIndex] ^= bitMask;

        // Handle intermediate words, if any
        if (endUnitIndex - startUnitIndex > 1) 
        {
            for(int i=startUnitIndex+1; i<endUnitIndex; i++)
            {
                _units[i] ^= WORD_MASK;
            }
        }

        // Handle last word
        bitMask = bits_right_of(toIndex & BIT_INDEX_MASK);
        _units[endUnitIndex] ^= bitMask;

        // Check to see if we reduced size
        if (_units[_units_in_use-1] == 0)
        {
            recalculate_units_in_use();
        }

        return 0;
    }



    /**
     * Sets the bit at the specified index to <code>true</code>.
     *
     * @param     bitIndex   a bit index.
     * @exception IndexOutOfBoundsException if the specified index is negative.
     * @since     JDK1.0
     */
    int set(int bitIndex)
    {
        if ((bitIndex < 0) || (bitIndex >=(int)_capacity))
        {
            return -1;
        }


        int unitIndex = unit_index(bitIndex);
        int unitsRequired = unitIndex + 1;

        if (_units_in_use < unitsRequired) 
        {
            _units_in_use = unitsRequired;        
            _units[unitIndex] |= bit(bitIndex);
        } 
        else 
        {
            _units[unitIndex] |= bit(bitIndex);
        }    

        return 0;
    }

    /**
     * Sets the bit at the specified index to the specified value.
     *
     * @param     bitIndex   a bit index.
     * @param     value a boolean value to set.
     * @exception IndexOutOfBoundsException if the specified index is negative.
     * @since     1.4
     */

    int set(int bitIndex, bool value)
    {
        if (value)
        {
            return set(bitIndex);
        }
        else
        {
            return clear(bitIndex);
        }
    }

    /**
     * Sets the bits from the specified fromIndex(inclusive) to the
     * specified toIndex(exclusive) to <code>true</code>.
     *
     * @param     fromIndex   index of the first bit to be set.
     * @param     toIndex index after the last bit to be set.
     * @exception IndexOutOfBoundsException if <tt>fromIndex</tt> is negative,
     *            or <tt>toIndex</tt> is negative, or <tt>fromIndex</tt> is
     *            larger than <tt>toIndex</tt>.
     * @since     1.4
     */
    int set(int fromIndex, int toIndex)
    {
        if ((fromIndex < 0)  || (fromIndex >=_capacity))
           return -1;
        if ((toIndex < 0) || (toIndex >=_capacity))
           return -1;
        if (fromIndex > toIndex)
           return -1;
        
        // Increase capacity if necessary
        int endUnitIndex = unit_index(toIndex);
        int unitsRequired = endUnitIndex + 1;

        if (_units_in_use < unitsRequired) 
        {
            _units_in_use = unitsRequired;
        }

        int startUnitIndex = unit_index(fromIndex);
//        long bitMask = 0;
        int bitMask = 0;
        if (startUnitIndex == endUnitIndex) 
        {
            // Case 1: One word
            bitMask = (1 << (toIndex & BIT_INDEX_MASK)) -
                      (1 << (fromIndex & BIT_INDEX_MASK));
            _units[startUnitIndex] |= bitMask;
            return 0;
         }
        
        // Case 2: Multiple words
        // Handle first word
        bitMask = bits_left_of(fromIndex & BIT_INDEX_MASK);
        _units[startUnitIndex] |= bitMask;

        // Handle intermediate words, if any
        if (endUnitIndex - startUnitIndex > 1) 
        {
            for(int i=startUnitIndex+1; i<endUnitIndex; i++)
            {
                _units[i] |= WORD_MASK;
            }
        }

        // Handle last word
        bitMask = bits_right_of(toIndex & BIT_INDEX_MASK);
        _units[endUnitIndex] |= bitMask;

        return 0;
    }

    /**
     * Sets the bits from the specified fromIndex(inclusive) to the
     * specified toIndex(exclusive) to the specified value.
     *
     * @param     fromIndex   index of the first bit to be set.
     * @param     toIndex index after the last bit to be set
     * @param     value value to set the selected bits to
     * @exception IndexOutOfBoundsException if <tt>fromIndex</tt> is negative,
     *            or <tt>toIndex</tt> is negative, or <tt>fromIndex</tt> is
     *            larger than <tt>toIndex</tt>.
     * @since     1.4
     */
    int set(int fromIndex, int toIndex, bool value)
    {
        if (value)
        {
            return set(fromIndex, toIndex);
        }
        else
        {
            return clear(fromIndex, toIndex);
        }
    }

    /**
     * Sets the bit specified by the index to <code>false</code>.
     *
     * @param     bitIndex   the index of the bit to be cleared.
     * @exception IndexOutOfBoundsException if the specified index is negative.
     * @since     JDK1.0
     */
    int  clear(int bitIndex)
    {
        if ((bitIndex < 0) || (bitIndex >=(int)_capacity))
        {
            return -1;
        }


	int unitIndex = unit_index(bitIndex);
	if (unitIndex >= _units_in_use)
       {   
	    return 0;
       }

	_units[unitIndex] &= ~bit(bitIndex);
        if (_units[_units_in_use-1] == 0)
        {
            recalculate_units_in_use();
        }

        return 0;
    }

    /**
     * Sets the bits from the specified fromIndex(inclusive) to the
     * specified toIndex(exclusive) to <code>false</code>.
     *
     * @param     fromIndex   index of the first bit to be cleared.
     * @param     toIndex index after the last bit to be cleared.
     * @exception IndexOutOfBoundsException if <tt>fromIndex</tt> is negative,
     *            or <tt>toIndex</tt> is negative, or <tt>fromIndex</tt> is
     *            larger than <tt>toIndex</tt>.
     * @since     1.4
     */
    int clear(int fromIndex, int toIndex)
    {
        
        if ((fromIndex < 0)  || (fromIndex >=_capacity))
           return -1;
        if ((toIndex < 0) || (toIndex >=_capacity))
           return -1;
        if (fromIndex > toIndex)
           return -1;
        
       int startUnitIndex = unit_index(fromIndex);
	if (startUnitIndex >= _units_in_use)
	    return;
        int endUnitIndex = unit_index(toIndex);

//        long bitMask = 0;
		int bitMask = 0;
        if (startUnitIndex == endUnitIndex) {
            // Case 1: One word
            bitMask = (1L << (toIndex & BIT_INDEX_MASK)) -
                      (1L << (fromIndex & BIT_INDEX_MASK));
            _units[startUnitIndex] &= ~bitMask;
            if (_units[_units_in_use-1] == 0)
            {
                recalculate_units_in_use();
            }
            return;
        }

        // Case 2: Multiple words
        // Handle first word
        bitMask = bits_left_of(fromIndex & BIT_INDEX_MASK);
        _units[startUnitIndex] &= ~bitMask;

        // Handle intermediate words, if any
        if (endUnitIndex - startUnitIndex > 1) 
        {
            for(int i=startUnitIndex+1; i<endUnitIndex; i++) 
            {
                if (i < _units_in_use)
                {
                    _units[i] = 0;
                }
            }
        }

        // Handle last word
        if (endUnitIndex < _units_in_use) 
        {
            bitMask = bits_right_of(toIndex & BIT_INDEX_MASK);
            _units[endUnitIndex] &= ~bitMask;
        }

        if (_units[_units_in_use-1] == 0)
        {
            recalculate_units_in_use();
        }

    }

    /**
     * Sets all of the bits in this BitSet to <code>false</code>.
     *
     * @since   1.4
     */
    void clear()
    {
        while (_units_in_use > 0)
        {
            _units[--_units_in_use] = 0;
        }
    }

    /**
     * Returns the value of the bit with the specified index. The value
     * is <code>true</code> if the bit with the index <code>bitIndex</code>
     * is currently set in this <code>BitSet</code>; otherwise, the result
     * is <code>false</code>.
     *
     * @param     bitIndex   the bit index.
     * @return    the value of the bit with the specified index.
     * @exception IndexOutOfBoundsException if the specified index is negative.
     */
    bool get(int bitIndex)
    { 
        /*if (bitIndex < 0)
            throw new IndexOutOfBoundsException("bitIndex < 0: " + bitIndex);
        */
	bool result = false;
	int unitIndex = unit_index(bitIndex);
	if (unitIndex < _units_in_use)
       {   
	    result = ((_units[unitIndex] & bit(bitIndex)) != 0);
       }

	return result;
    }

    /**
     * Returns a new <tt>BitSet</tt> composed of bits from this <tt>BitSet</tt>
     * from <tt>fromIndex</tt>(inclusive) to <tt>toIndex</tt>(exclusive).
     *
     * @param     fromIndex   index of the first bit to include.
     * @param     toIndex     index after the last bit to include.
     * @return    a new <tt>BitSet</tt> from a range of this <tt>BitSet</tt>.
     * @exception IndexOutOfBoundsException if <tt>fromIndex</tt> is negative,
     *            or <tt>toIndex</tt> is negative, or <tt>fromIndex</tt> is
     *            larger than <tt>toIndex</tt>.
     * @since   1.4
     */
    BitSet get(int fromIndex, int toIndex)
    {
        /*
        if (fromIndex < 0)
            throw new IndexOutOfBoundsException("fromIndex < 0: " + fromIndex);
        if (toIndex < 0)
            throw new IndexOutOfBoundsException("toIndex < 0: " + toIndex);
        if (fromIndex > toIndex)
            throw new IndexOutOfBoundsException("fromIndex: " + fromIndex +
                                                " > toIndex: " + toIndex);
        */
        // If no set bits in range return empty bitset
        if (length() <= fromIndex || fromIndex == toIndex)
            return new BitSet(0);

        // An optimization
        if (length() < toIndex)
            toIndex = length();

        BitSet result = new BitSet(toIndex - fromIndex);
        int startBitIndex = fromIndex & BIT_INDEX_MASK;
        int endBitIndex = toIndex & BIT_INDEX_MASK;
        int targetWords = (toIndex - fromIndex + 63)/64;
        int sourceWords = unit_index(toIndex) - unit_index(fromIndex) + 1;
        int inverseIndex = 64 - startBitIndex;
        int targetIndex = 0;
        int sourceIndex = unit_index(fromIndex);

        // Process all words but the last word
        while (targetIndex < targetWords - 1)
        {
            result._units[targetIndex++] =
                (_units[sourceIndex++] >> startBitIndex) |
                ((inverseIndex==64) ? 0 : _units[sourceIndex] << inverseIndex);
        }

        // Process the last word
        result._units[targetIndex] = (sourceWords == targetWords ?
                                      (_units[sourceIndex] & bits_right_of(endBitIndex)) >> startBitIndex :
                                      (_units[sourceIndex++] >> startBitIndex) | ((inverseIndex==64) ? 0 :
                                              (get_bits(sourceIndex) & bits_right_of(endBitIndex)) << inverseIndex));

        // Set _units_in_use correctly
        result._units_in_use = targetWords;
        result.recalculate_units_in_use();
        return result;
    }



    /**
     * Returns the index of the first bit that is set to <code>true</code>
     * that occurs on or after the specified starting index. If no such
     * bit exists then -1 is returned.
     *
     * To iterate over the <code>true</code> bits in a <code>BitSet</code>,
     * use the following loop:
     *
     * for(int i=bs.next_set_bit(0); i>=0; i=bs.next_set_bit(i+1)) {
     *     // operate on index i here
     * }
     *
     * @param   fromIndex the index to start checking from (inclusive).
     * @return  the index of the next set bit.
     * @throws  IndexOutOfBoundsException if the specified index is negative.
     * @since   1.4
     */
    int next_set_bit(int fromIndex)
    {
        if ((fromIndex < 0) || (fromIndex >=(int)_capacity))
        {
            return -1;
        }

        int u = unit_index(fromIndex);
        if (u >= _units_in_use)
            return -1;
        int testIndex = (fromIndex & BIT_INDEX_MASK);
        uint32_t unit = _units[u] >> testIndex;

        if (unit == 0)
        {
            testIndex = 0;
        }

        while((unit==0) && (u < _units_in_use-1))
        {
            unit = _units[++u];
        }

        if (unit == 0)
        {
            return -1;
        }

        testIndex  += trailing_zero_cnt(unit);
        return ((u * BITS_PER_UNIT) + testIndex);
    }



    /**
     * Returns the index of the first bit that is set to <code>false</code>
     * that occurs on or after the specified starting index.
     *
     * @param   fromIndex the index to start checking from (inclusive).
     * @return  the index of the next clear bit.
     * @throws  IndexOutOfBoundsException if the specified index is negative.
     * @since   1.4
     */
    int next_clear_bit(int fromIndex)
    {
        if ((fromIndex < 0) || (fromIndex >=_capacity))
        {
            return -1;
        }

        int u = unit_index(fromIndex);
        if (u >= _units_in_use)
        {
            return fromIndex;
        }

        int testIndex = (fromIndex & BIT_INDEX_MASK);
        uint32_t unit = _units[u] >> testIndex;

        if (unit == (WORD_MASK >> testIndex))
        {
            testIndex = 0;
        }

        while((unit==WORD_MASK) && (u < _units_in_use-1))
        {
            unit = _units[++u];
        }

        if (unit == WORD_MASK)
        {
            return length();
        }

        if (unit == 0)
        {
            return u * BITS_PER_UNIT + testIndex;
        }

        testIndex += trailing_zero_cnt(~unit);
        return ((u * BITS_PER_UNIT) + testIndex);
    }

    /**
     * Returns the "logical size" of this <code>BitSet</code>: the index of
     * the highest set bit in the <code>BitSet</code> plus one. Returns zero
     * if the <code>BitSet</code> contains no set bits.
     *
     * @return  the logical size of this <code>BitSet</code>.
     * @since   1.2
     */
    int length()
    {
        if (_units_in_use == 0)
        {
            return 0;
        }

        uint32_t highestUnit = _units[_units_in_use - 1];

        return 32 * (_units_in_use - 1) + bitLen((int)highestUnit);
    }


    /**
     * Returns true if this <code>BitSet</code> contains no bits that are set
     * to <code>true</code>.
     *
     * @return    boolean indicating whether this <code>BitSet</code> is empty.
     * @since     1.4
     */
    bool is_empty()
    {
        return (_units_in_use == 0);
    }

    /**
     * Returns true if the specified <code>BitSet</code> has any bits set to
     * <code>true</code> that are also set to <code>true</code> in this
     * <code>BitSet</code>.
     *
     * @param	set <code>BitSet</code> to intersect with
     * @return  boolean indicating whether this <code>BitSet</code> intersects
     *          the specified <code>BitSet</code>.
     * @since   1.4
     */
    bool intersects(BitSet set)
    {
        for(int i = math_min(_units_in_use, set._units_in_use)-1; i>=0; i--)
        {
            if ((_units[i] & set._units[i]) != 0)
            {
                return true;
            }
        }
        return false;
    }

    /**
     * Returns the number of bits set to <tt>true</tt> in this
     * <code>BitSet</code>.
     *
     * @return  the number of bits set to <tt>true</tt> in this
     *          <code>BitSet</code>.
     * @since   1.4
     */
    int cardinality()
    {
        int sum = 0;
        for (int i=0; i<_units_in_use; i++)
        {
            sum += bit_count(_units[i]);
        }
        return sum;
    }

    /**
     * Performs a logical <b>AND</b> of this target bit set with the
     * argument bit set. This bit set is modified so that each bit in it
     * has the value <code>true</code> if and only if it both initially
     * had the value <code>true</code> and the corresponding bit in the
     * bit set argument also had the value <code>true</code>.
     *
     * @param   set   a bit set.
     */
    void do_and(BitSet& set)
    {
        if (this == &set)
        {
            return;
        }

        // Perform logical AND on bits in common
        int oldUnitsInUse = _units_in_use;
        _units_in_use = math_min(_units_in_use, set._units_in_use);
        int i;
        for(i=0; i<_units_in_use; i++)
        {
            _units[i] &= set._units[i];
        }

        // Clear out units no uint32_ter used
        for( ; i < oldUnitsInUse; i++)
        {
            _units[i] = 0;
        }

        // Recalculate units in use if necessary
        if (_units_in_use > 0 && _units[_units_in_use - 1] == 0)
        {
            recalculate_units_in_use();
        }
    }

    /**
     * Performs a logical <b>OR</b> of this bit set with the bit set
     * argument. This bit set is modified so that a bit in it has the
     * value <code>true</code> if and only if it either already had the
     * value <code>true</code> or the corresponding bit in the bit set
     * argument has the value <code>true</code>.
     *
     * @param   set   a bit set.
     */
    void do_or(BitSet& set)
    {
        if (this == &set)
        {
            return;
        }

        ensure_capacity(set._units_in_use);

        // Perform logical OR on bits in common
        int unitsInCommon = math_min(_units_in_use, set._units_in_use);
        int i;
        for(i=0; i<unitsInCommon; i++)
        {
            _units[i] |= set._units[i];
        }

        // Copy any remaining bits
        for(; i<set._units_in_use; i++)
        {
            _units[i] = set._units[i];
        }

        if (_units_in_use < set._units_in_use)
        {
            _units_in_use = set._units_in_use;
        }
    }

    /**
     * Performs a logical <b>XOR</b> of this bit set with the bit set
     * argument. This bit set is modified so that a bit in it has the
     * value <code>true</code> if and only if one of the following
     * statements holds:
     * <ul>
     * <li>The bit initially has the value <code>true</code>, and the
     *     corresponding bit in the argument has the value <code>false</code>.
     * <li>The bit initially has the value <code>false</code>, and the
     *     corresponding bit in the argument has the value <code>true</code>.
     * </ul>
     *
     * @param   set   a bit set.
     */
    void do_xor(BitSet& set)
    {
        int unitsInCommon;

        if (_units_in_use >= set._units_in_use)
        {
            unitsInCommon = set._units_in_use;
        }
        else
        {
            unitsInCommon = _units_in_use;
            int newUnitsInUse = set._units_in_use;

            _units_in_use = newUnitsInUse;
        }

        // Perform logical XOR on bits in common
        int i;
        for (i=0; i<unitsInCommon; i++)
        {
            _units[i] ^= set._units[i];
        }

        // Copy any remaining bits
        for ( ; i<set._units_in_use; i++)
        {
            _units[i] = set._units[i];
        }

        recalculate_units_in_use();
    }

    /**
     * Clears all of the bits in this <code>BitSet</code> whose corresponding
     * bit is set in the specified <code>BitSet</code>.
     *
     * @param     set the <code>BitSet</code> with which to mask this
     *            <code>BitSet</code>.
     * @since     JDK1.2
     */
    void do_and_not(BitSet set)
    {
        int unitsInCommon = math_min(_units_in_use, set._units_in_use);

        // Perform logical (a & !b) on bits in common
        for (int i=0; i<unitsInCommon; i++)
        {
            _units[i] &= ~set._units[i];
        }

        recalculate_units_in_use();
    }

    /**
     * Returns a hash code value for this bit set. The has code
     * depends only on which bits have been set within this
     * <code>BitSet</code>. The algorithm used to compute it may
     * be described as follows.<p>
     * Suppose the bits in the <code>BitSet</code> were to be stored
     * in an array of <code>uint32_t</code> integers called, say,
     * <code>bits</code>, in such a manner that bit <code>k</code> is
     * set in the <code>BitSet</code> (for nonnegative values of
     * <code>k</code>) if and only if the expression
     * <pre>((k&gt;&gt;6) &lt; bits.length) && ((bits[k&gt;&gt;6] & (1 &lt;&lt; (bit & 0x3F))) != 0)</pre>
     * is true. Then the following definition of the <code>hash_code</code>
     * method would be a correct implementation of the actual algorithm:
     * <pre>
     * public int hash_code() {
     *      uint32_t h = 1234;
     *      for (int i = bits.length; --i &gt;= 0; ) {
     *           h ^= bits[i] * (i + 1);
     *      }
     *      return (int)((h &gt;&gt; 32) ^ h);
     * }</pre>
     * Note that the hash code values change if the set of bits is altered.
     * <p>Overrides the <code>hash_code</code> method of <code>Object</code>.
     *
     * @return  a hash code value for this bit set.
     */
    int hash_code()
    {
    /*
        uint32_t h = 1234;
        for (int i = _units.length; --i >= 0; )
        {
            h ^= _units[i] * (i + 1);
        }

        return (int)((h >> 32) ^ h);
        */

    return 0;
    }

    /**
     * Returns the number of bits of space actually in use by this
     * <code>BitSet</code> to represent bit values.
     * The maximum element in the set is the size - 1st element.
     *
     * @return  the number of bits currently in this bit set.
     */
    int size()
    {
        return _capacity;
    }

    /**
     * Compares this object against the specified object.
     * The result is <code>true</code> if and only if the argument is
     * not <code>null</code> and is a <code>Bitset</code> object that has
     * exactly the same set of bits set to <code>true</code> as this bit
     * set. That is, for every nonnegative <code>int</code> index <code>k</code>,
     * <pre>((BitSet)obj).get(k) == this.get(k)</pre>
     * must be true. The current sizes of the two bit sets are not compared.
     * <p>Overrides the <code>equals</code> method of <code>Object</code>.
     *
     * @param   obj   the object to compare with.
     * @return  <code>true</code> if the objects are the same;
     *          <code>false</code> otherwise.
     * @see     java.util.BitSet#size()
     */
    bool equals(BitSet& obj)
    {
        if (this == &obj)
        {
            return true;
        }

        BitSet set = (BitSet) obj;
        int minUnitsInUse = math_min(_units_in_use, set._units_in_use);

        // Check units in use by both BitSets
        for (int i = 0; i < minUnitsInUse; i++)
        {
            if (_units[i] != set._units[i])
            {
                return false;

            }
        }

        // Check any units in use by only one BitSet (must be 0 in other)
        if (_units_in_use > minUnitsInUse)
        {
            for (int i = minUnitsInUse; i<_units_in_use; i++)
            {
                if (_units[i] != 0)
                {
                    return false;
                }
            }
        }
        else
        {
            for (int i = minUnitsInUse; i<set._units_in_use; i++)
            {
                if (set._units[i] != 0)
                {
                    return false;
                }
            }
        }

        return true;
    }

    /**
     * Cloning this <code>BitSet</code> produces a new <code>BitSet</code>
     * that is equal to it.
     * The clone of the bit set is another bit set that has exactly the
     * same bits set to <code>true</code> as this bit set and the same
     * current size.
     * <p>Overrides the <code>clone</code> method of <code>Object</code>.
     *
     * @return  a clone of this bit set.
     * @see     java.util.BitSet#size()
     */
    /*
    Object clone()
    {
       BitSet result = null;
       try {
           result = (BitSet) super.clone();
       } catch (CloneNotSupportedException e) {
           throw new InternalError();
       }
       result._units = new uint32_t[_units.length];
       System.arraycopy(_units, 0, result._units, 0, _units_in_use);
       return result;
    }
    */


    /**
     * Returns a string representation of this bit set. For every index
     * for which this <code>BitSet</code> contains a bit in the set
     * state, the decimal representation of that index is included in
     * the result. Such indices are listed in order from lowest to
     * highest, separated by ",&nbsp;" (a comma and a space) and
     * surrounded by braces, resulting in the usual mathematical
     * notation for a set of integers.<p>
     * Overrides the <code>toString</code> method of <code>Object</code>.
     * <p>Example:
     * <pre>
     * BitSet drPepper = new BitSet();</pre>
     * Now <code>drPepper.toString()</code> returns "<code>{}</code>".<p>
     * <pre>
     * drPepper.set(2);</pre>
     * Now <code>drPepper.toString()</code> returns "<code>{2}</code>".<p>
     * <pre>
     * drPepper.set(4);
     * drPepper.set(10);</pre>
     * Now <code>drPepper.toString()</code> returns "<code>{2, 4, 10}</code>".
     *
     * @return  a string representation of this bit set.
     */
    /*
    string to_string()
    {

       int numBits = _units_in_use << ADDRESS_BITS_PER_UNIT;
       StringBuffer buffer = new StringBuffer(8*numBits + 2);
       String separator = "";
       buffer.append('{');

       for (int i = 0 ; i < numBits; i++)
       {
           if (get(i)) {
               buffer.append(separator);
               separator = ", ";
               buffer.append(i);
           }
       }

       buffer.append('}');
       return buffer.toString();
    }
    */
    /**
     * The bits in this BitSet.  The ith bit is stored in bits[i/64] at
     * bit position i % 64 (where bit position 0 refers to the least
     * significant bit and 63 refers to the most significant bit).
     * INVARIANT: The words in bits[] above _units_in_use-1 are zero.
     *
     * @serial
     */

    
private:

    uint32_t  _capacity;

    uint32_t _units[(((capacity-1) >> ADDRESS_BITS_PER_UNIT)+ 1)];
    
    /**
     * The number of units in the logical size of this BitSet.
     * INVARIANT: _units_in_use is nonnegative.
     * INVARIANT: bits[_units_in_use-1] is nonzero unless _units_in_use is zero.
     */
    int _units_capacity;
    int _units_in_use;

};

#endif
