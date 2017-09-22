#ifndef MEMORY_H_
#define MEMORY_H_
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

void *mmalloc(size_t size);
void mfree(void *ptr);
void *mcalloc(size_t nmemb, size_t size);
void *mrealloc(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif
