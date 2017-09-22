/**
 * \file
 * \brief 多字节字符的拷贝和查找
 */
#ifndef _MB_SRING_H_
#define  _MB_SRING_H_

#include <ctype.h>

char *  mbsnbcpy(char *dst,  const  char *src, size_t& cnt);
unsigned char * mbschr(const unsigned char *string,  unsigned int c);

#endif

