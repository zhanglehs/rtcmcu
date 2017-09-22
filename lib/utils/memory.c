#include "memory.h"

#include <stdlib.h>

void *
mmalloc(size_t size)
{
    return malloc(size);
}

void
mfree(void *ptr)
{
    return free(ptr);
}

void *
mcalloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

void *
mrealloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}
