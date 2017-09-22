#ifndef REPORT_H_
#define REPORT_H_
#include <stdint.h>
#include "common.h"

#if (defined __cplusplus && !defined _WIN32)
extern "C"
{
#endif
int report_init(const char *path, const char *server_role,
                const char *version, const char *ip, uint16_t port);
void
report_write(const char *module, const char *format, ...)
ATTR_PRINTF(2, 3);
     void report_fini();

#define REPORT(module, ...) do {\
    report_write(module, __VA_ARGS__);\
}while(0)

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
