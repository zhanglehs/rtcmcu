#ifndef COMMON_H_
#define COMMON_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define COUNT_OF(array) (sizeof(array)/sizeof((array)[0]))
#define ATTR_PRINTF(n, m) __attribute__((format(printf, n, m)))
#define MAX(a,b) (a > b ? a : b)
#define MIN(a,b) (a < b ? a : b)
#define UNUSED(a) ((void)(a));

#ifndef container_of
#define container_of(ptr, type, member) ({			\
		(type *)( (char *)ptr - offsetof(type,member) );})
#endif
typedef int boolean;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef likely
#ifdef __builtin_expect
#define likely(x) __builtin_expect((x),1)
#else
#define likely(x) (x)
#endif
#endif

#ifndef unlikely
#ifdef __builtin_expect
#define unlikely(x) __builtin_expect((x),0)
#else
#define unlikely(x) (x)
#endif
#endif


#ifdef __cplusplus
}
#endif
#endif
