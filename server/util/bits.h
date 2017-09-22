#ifndef __BITS_H__
#define __BITS_H__

#ifdef __cplusplus
extern "C" {
#endif


#include <stdio.h>
#include "stdint.h"
#define  INLINE inline


#define BYTE_NUMBIT     8
#define BYTE_NUMBIT_LD  3
#define bit2byte(a) ((a+7)>>BYTE_NUMBIT_LD)

    typedef struct _bitfile
    {
        /* bit input */
        uint32_t bufa;
        uint32_t bufb;
        uint32_t bits_left;
        uint32_t buffer_size; /* size of the buffer in bytes */
        uint32_t bytes_left;
        uint8_t error;
        uint32_t *tail;
        uint32_t *start;
        const void *buffer;
    } bitfile;


    void bitbuffer_initbits(bitfile *ld, const void *buffer, const uint32_t buffer_size);
    void bitbuffer_endbits(bitfile *ld);
    void bitbuffer_initbits_rev(bitfile *ld, void *buffer,
        uint32_t bits_in_buffer);
    uint8_t bitbuffer_byte_align(bitfile *ld);
    uint32_t bitbuffer_get_processed_bits(bitfile *ld);
    void bitbuffer_flushbits_ex(bitfile *ld, uint32_t bits);
    void bitbuffer_rewindbits(bitfile *ld);
    void bitbuffer_resetbits(bitfile *ld, int bits);
    uint8_t *bitbuffer_getbitbuffer(bitfile *ld, uint32_t bits);

    /* return next n bits (right adjusted) */
    uint32_t bitbuffer_getbits(bitfile *ld, uint32_t n);

    uint8_t bitbuffer_get1bit(bitfile *ld);

    /* reversed bitreading routines */
    //static  uint32_t bitbuffer_showbits_rev(bitfile *ld, uint32_t bits)
    //{
    //    uint8_t i;
    //    uint32_t B = 0;

    //    if (bits <= ld->bits_left)
    //    {
    //        for (i = 0; i < bits; i++)
    //        {
    //            if (ld->bufa & (1 << (i + (32 - ld->bits_left))))
    //                B |= (1 << (bits - i - 1));
    //        }
    //        return B;
    //    }
    //    else {
    //        for (i = 0; i < ld->bits_left; i++)
    //        {
    //            if (ld->bufa & (1 << (i + (32 - ld->bits_left))))
    //                B |= (1 << (bits - i - 1));
    //        }
    //        for (i = 0; i < bits - ld->bits_left; i++)
    //        {
    //            if (ld->bufb & (1 << (i + (32 - ld->bits_left))))
    //                B |= (1 << (bits - ld->bits_left - i - 1));
    //        }
    //        return B;
    //    }
    //}

    //static void bitbuffer_flushbits_rev(bitfile *ld, uint32_t bits)
    //{
    //    /* do nothing if error */
    //    if (ld->error != 0)
    //        return;

    //    if (bits < ld->bits_left)
    //    {
    //        ld->bits_left -= bits;
    //    }
    //    else {
    //        uint32_t tmp;

    //        ld->bufa = ld->bufb;
    //        tmp = getdword(ld->start);
    //        ld->bufb = tmp;
    //        ld->start--;
    //        ld->bits_left += (32 - bits);

    //        if (ld->bytes_left < 4)
    //        {
    //            ld->error = 1;
    //            ld->bytes_left = 0;
    //        }
    //        else {
    //            ld->bytes_left -= 4;
    //        }
    //    }
    //}

    //static uint32_t bitbuffer_getbits_rev(bitfile *ld, uint32_t n)
    //{
    //    uint32_t ret;

    //    if (n == 0)
    //        return 0;

    //    ret = bitbuffer_showbits_rev(ld, n);
    //    bitbuffer_flushbits_rev(ld, n);

    //    return ret;
    //}

#ifdef __cplusplus
}
#endif
#endif
