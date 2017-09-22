#ifndef __LCDN_UTIL_H
#define __LCDN_UTIL_H


#if (defined __cplusplus && !defined _WIN32)
extern "C" 
{
#endif

int util_create_pid_file(const char *path, const char *process_name);


#if (defined __cplusplus && !defined _WIN32)
}
#endif

#endif

