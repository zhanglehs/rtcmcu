#include "backtrace.h"

#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>
#include <string.h>

#define MAX_BUF_SZ   128

static char g_dir[PATH_MAX];
static char g_file_path[PATH_MAX];
static char g_process_name[PATH_MAX];
static char g_version[64];

struct trace_sig_t
{
	int signum;
	//void *old_handler;
	sighandler_t old_handler;
};

static struct trace_sig_t g_trace_sig[] = {
	{ SIGABRT, NULL },
	{ SIGBUS, NULL },
	{ SIGFPE, NULL },
	{ SIGILL, NULL },
	{ SIGIOT, NULL },
	{ SIGSEGV, NULL },
	{ SIGTRAP, NULL }
};

void
handle_whitespace(char *s)
{
	char *c = s;

	for (; *c != '\0'; c++) {
		if (*c == ' ') {
			*c = '-';
		}
	}
}

static int
creat_file()
{

	int fd = -1;
	int pid = getpid();

	snprintf(g_file_path, sizeof(g_file_path)-1, "%s/%s.%d.%s", g_dir,
		g_process_name, pid, g_version);
	handle_whitespace(g_file_path);
	fd = creat(g_file_path, 0644);

	return fd;
}

static void
signal_handler(int signum)
{
	int fd = creat_file();
	void *buf[MAX_BUF_SZ];

	signal(signum, SIG_DFL);
	if (-1 != fd) {
		int real_len = backtrace(buf, MAX_BUF_SZ);

		backtrace_symbols_fd(buf, real_len, fd);

		close(fd);
	}
	if (signum != SIGTRAP) {
		kill(getpid(), signum);
	}
	else {
		signal(signum, signal_handler);
	}
}

int
backtrace_init(const char *dir, const char *process_name, const char *version)
{
	int i;

	if (NULL == realpath(dir, g_dir)) {
		fprintf(stderr, "%s:%d realpath failed. error = %s", __FILE__,
			__LINE__, strerror(errno));
		return -1;
	}
	strncpy(g_version, version, sizeof(g_version));
	strncpy(g_process_name, process_name, sizeof(g_process_name)-1);

	for (i = 0; i < (int)(sizeof(g_trace_sig) / sizeof(g_trace_sig[0])); i++) {
		struct trace_sig_t *sig = g_trace_sig + i;

		sig->old_handler = signal(sig->signum, signal_handler);
	}

	return 0;
}

int
backtrace_fini()
{
	int i;

	for (i = 0; i < (int)(sizeof(g_trace_sig) / sizeof(g_trace_sig[0])); i++) {
		struct trace_sig_t *sig = g_trace_sig + i;

		if (SIG_ERR != sig->old_handler)
			signal(sig->signum, sig->old_handler);
	}

	return 0;
}
