/*
 * author: hechao@youku.com
 * create: 2013.7.21
 */

#ifndef UTIL_STATFILE_H
#define UTIL_STATFILE_H

#if (defined __cplusplus && !defined _WIN32)
extern "C"
{
#endif
int open_statfile_or_pipe(const char *statfile);

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
