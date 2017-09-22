#include <stdlib.h>
#include "bits.h"

static uint32_t bitbuffer_showbits(bitfile *ld, uint32_t bits)
{
  if (bits <= ld->bits_left)
  {
    //return (ld->bufa >> (ld->bits_left - bits)) & bitmask[bits];
    return (ld->bufa << (32 - ld->bits_left)) >> (32 - bits);
  }

  bits -= ld->bits_left;
  //return ((ld->bufa & bitmask[ld->bits_left]) << bits) | (ld->bufb >> (32 - bits));
  return ((ld->bufa & ((1 << ld->bits_left) - 1)) << bits) | (ld->bufb >> (32 - bits));
}

static void bitbuffer_flushbits(bitfile *ld, uint32_t bits)
{
  /* do nothing if error */
  if (ld->error != 0)
    return;

  if (bits < ld->bits_left)
  {
    ld->bits_left -= bits;
  }
  else {
    bitbuffer_flushbits_ex(ld, bits);
  }
}

/* reads only n bytes from the stream instead of the standard 4 */
static /*INLINE*/ uint32_t getdword_n(void *mem, int n)
{
  uint32_t tmp = 0;
#ifndef ARCH_IS_BIG_ENDIAN
  switch (n)
  {
  case 3:
    ((uint8_t*)&tmp)[1] = ((uint8_t*)mem)[2];
  case 2:
    ((uint8_t*)&tmp)[2] = ((uint8_t*)mem)[1];
  case 1:
    ((uint8_t*)&tmp)[3] = ((uint8_t*)mem)[0];
  default:
    break;
  }
#else
  switch (n)
  {
  case 3:
    ((uint8_t*)&tmp)[2] = ((uint8_t*)mem)[2];
  case 2:
    ((uint8_t*)&tmp)[1] = ((uint8_t*)mem)[1];
  case 1:
    ((uint8_t*)&tmp)[0] = ((uint8_t*)mem)[0];
  default:
    break;
  }
#endif

  return tmp;
}

/* circumvent memory alignment errors on ARM */
static  uint32_t getdword(void *mem)
{
  uint32_t tmp;
#ifndef ARCH_IS_BIG_ENDIAN
  ((uint8_t*)&tmp)[0] = ((uint8_t*)mem)[3];
  ((uint8_t*)&tmp)[1] = ((uint8_t*)mem)[2];
  ((uint8_t*)&tmp)[2] = ((uint8_t*)mem)[1];
  ((uint8_t*)&tmp)[3] = ((uint8_t*)mem)[0];
#else
  ((uint8_t*)&tmp)[0] = ((uint8_t*)mem)[0];
  ((uint8_t*)&tmp)[1] = ((uint8_t*)mem)[1];
  ((uint8_t*)&tmp)[2] = ((uint8_t*)mem)[2];
  ((uint8_t*)&tmp)[3] = ((uint8_t*)mem)[3];
#endif

  return tmp;
}

/* initialize buffer, call once before first getbits or showbits */
void bitbuffer_initbits(bitfile *ld, const void *_buffer, const uint32_t buffer_size)
{
    uint32_t tmp;

    if (ld == NULL)
        return;


    if (buffer_size == 0 || _buffer == NULL)
    {
        ld->error = 1;
        return;
    }

    ld->buffer = _buffer;

    ld->buffer_size = buffer_size;
    ld->bytes_left = buffer_size;

    if (ld->bytes_left >= 4)
    {
        tmp = getdword((uint32_t*)ld->buffer);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n((uint32_t*)ld->buffer, ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufa = tmp;

    if (ld->bytes_left >= 4)
    {
        tmp = getdword((uint32_t*)ld->buffer + 1);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n((uint32_t*)ld->buffer + 1, ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufb = tmp;

    ld->start = (uint32_t*)ld->buffer;
    ld->tail = ((uint32_t*)ld->buffer + 2);

    ld->bits_left = 32;

    ld->error = 0;
}

void bitbuffer_endbits(bitfile *ld)
{
    // void
}

uint32_t bitbuffer_get_processed_bits(bitfile *ld)
{
    return (uint32_t)(8 * (4 * (ld->tail - ld->start) - 4) - (ld->bits_left));
}

uint8_t bitbuffer_byte_align(bitfile *ld)
{
    int remainder = (32 - ld->bits_left) & 0x7;

    if (remainder)
    {
        bitbuffer_flushbits(ld, 8 - remainder);
        return (uint8_t)(8 - remainder);
    }
    return 0;
}

void bitbuffer_flushbits_ex(bitfile *ld, uint32_t bits)
{
    uint32_t tmp;

    ld->bufa = ld->bufb;
    if (ld->bytes_left >= 4)
    {
        tmp = getdword(ld->tail);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n(ld->tail, ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufb = tmp;
    ld->tail++;
    ld->bits_left += (32 - bits);
}

/* rewind to beginning */
void bitbuffer_rewindbits(bitfile *ld)
{
    uint32_t tmp;

    ld->bytes_left = ld->buffer_size;

    if (ld->bytes_left >= 4)
    {
        tmp = getdword((uint32_t*)&ld->start[0]);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n((uint32_t*)&ld->start[0], ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufa = tmp;

    if (ld->bytes_left >= 4)
    {
        tmp = getdword((uint32_t*)&ld->start[1]);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n((uint32_t*)&ld->start[1], ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufb = tmp;

    ld->bits_left = 32;
    ld->tail = &ld->start[2];
}

/* reset to a certain point */
void bitbuffer_resetbits(bitfile *ld, int bits)
{
    uint32_t tmp;
    int words = bits >> 5;
    int remainder = bits & 0x1F;

    ld->bytes_left = ld->buffer_size - words * 4;

    if (ld->bytes_left >= 4)
    {
        tmp = getdword(&ld->start[words]);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n(&ld->start[words], ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufa = tmp;

    if (ld->bytes_left >= 4)
    {
        tmp = getdword(&ld->start[words + 1]);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n(&ld->start[words + 1], ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufb = tmp;

    ld->bits_left = 32 - remainder;
    ld->tail = &ld->start[words + 2];

    /* recheck for reading too many bytes */
    ld->error = 0;
}

uint8_t *bitbuffer_getbitbuffer(bitfile *ld, uint32_t bits)
{
    int i;
    unsigned int temp;
    int bytes = bits >> 3;
    int remainder = bits & 0x7;

    uint8_t *buffer = (uint8_t*)malloc((bytes + 1)*sizeof(uint8_t));

    for (i = 0; i < bytes; i++)
    {
        buffer[i] = (uint8_t)bitbuffer_getbits(ld, 8);
    }

    if (remainder)
    {
        temp = bitbuffer_getbits(ld, remainder) << (8 - remainder);

        buffer[bytes] = (uint8_t)temp;
    }

    return buffer;
}


/* reversed bit reading routines, used for RVLC and HCR */
void bitbuffer_initbits_rev(bitfile *ld, void *buffer,
    uint32_t bits_in_buffer)
{
    uint32_t tmp;
    int32_t index;

    ld->buffer_size = bit2byte(bits_in_buffer);

    index = (bits_in_buffer + 31) / 32 - 1;

    ld->start = (uint32_t*)buffer + index - 2;

    tmp = getdword((uint32_t*)buffer + index);
    ld->bufa = tmp;

    tmp = getdword((uint32_t*)buffer + index - 1);
    ld->bufb = tmp;

    ld->tail = (uint32_t*)buffer + index;

    ld->bits_left = bits_in_buffer % 32;
    if (ld->bits_left == 0)
        ld->bits_left = 32;

    ld->bytes_left = ld->buffer_size;
    ld->error = 0;
}

uint8_t bitbuffer_get1bit(bitfile *ld)
{
  uint8_t r;

  if (ld->bits_left > 0)
  {
    ld->bits_left--;
    r = (uint8_t)((ld->bufa >> ld->bits_left) & 1);
    return r;
  }

  r = (uint8_t)bitbuffer_getbits(ld, 1);
  return r;
}

uint32_t bitbuffer_getbits(bitfile *ld, uint32_t n)
{
  uint32_t ret;

  if (n == 0)
    return 0;

  ret = bitbuffer_showbits(ld, n);
  bitbuffer_flushbits(ld, n);

  return ret;
}

/* EOF */
