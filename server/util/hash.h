#ifndef __HASH_H_
#define __HASH_H_

#include "stdint.h"
#include "stddef.h"
uint32_t murmur_hash(const char *key, size_t len);

#endif
