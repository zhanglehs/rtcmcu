#ifndef ACCESS_H_
#define ACCESS_H_
#include <stddef.h>
#include <stdint.h>
#include "common.h"
#include <time.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include "statfile.h"

#if (defined __cplusplus && !defined _WIN32)
extern "C"
{
#endif
	enum
	{
		ACCESS_MODE_FILE = 1,
		ACCESS_MODE_NET = 2,
	};

	typedef struct access_config
	{
		int access_fd;
		int mode;
		struct tm last_day;
		size_t max_size;
		char base_path[PATH_MAX];
		char file_path[PATH_MAX];
		char prefix_tags[1024];
	} access_config;

	extern access_config g_conf_access;

	int access_init_file(const char *basepath, const char *name_prefix,
		size_t max_size);

	int access_init_net(const char *remotepath, const char *server_role,
		const char *ip, uint16_t port);

	void
		access_print(const char *format, ...)
		ATTR_PRINTF(1, 2);

	void access_fini();

	void access_set_max_size(size_t max_size);

#define ACCESS(...) do { \
	access_print(__VA_ARGS__);	\
	} while(0)

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif