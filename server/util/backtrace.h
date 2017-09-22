#ifndef BACKTRACE_H_
#define BACKTRACE_H_

#if (defined __cplusplus && !defined _WIN32)
extern "C"
{
#endif

    int backtrace_init(const char *dir, const char *process_name,
                       const char *version);
    int backtrace_fini();

#if (defined __cplusplus && !defined _WIN32)
}
#endif

#endif                          // BACKTRACE_H
