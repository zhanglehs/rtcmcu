#ifndef COMMON_H_
#define COMMON_H_
#include <stdint.h>

#if (defined __cplusplus && !defined _WIN32)
extern "C"
{
#endif
#define COUNT_OF(array) (sizeof(array)/sizeof((array)[0]))
#define ATTR_PRINTF(n, m) __attribute__((format(printf, n, m)))
#ifndef MAX
#define MAX(a,b) ( (a) > (b) ? (a) : (b) )
#endif
#ifndef MIN
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#endif
#define UNUSED(a) ((void)(a));
#ifndef SAFE_RELEASE
#define SAFE_DELETE( x ) \
    if (NULL != x) \
    { \
    delete x; \
    x = NULL; \
}
#endif

#ifndef container_of
#define container_of(ptr, type, member) ({			\
	(type *)((char *)ptr - offsetof(type, member)); })
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

#ifdef _WIN32
#define __null 0
#endif

#ifndef PROCESS_NAME
#define PROCESS_NAME "receiver"
#endif

#ifndef SVN_REV
#define SVN_REV "0.0.0.0"
#endif

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
