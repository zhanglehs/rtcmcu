#include "report.h"
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "statfile.h"
#include "log.h"

typedef struct report_ctx
{
	int fd;
	char prefix_tags[1024];
	char version[1024];
} report_ctx;

static report_ctx g_report_ctx;

int
report_init(const char *path, const char *server_role, const char *version,
const char *ip, uint16_t port)
{
	memset(&g_report_ctx, 0, sizeof(g_report_ctx));
	g_report_ctx.fd = -1;

	g_report_ctx.fd = open_statfile_or_pipe(path);
	if (g_report_ctx.fd < 0) {
		ERR("open stat file failed.");
		return -1;
	}

	snprintf(g_report_ctx.prefix_tags, sizeof(g_report_ctx.prefix_tags) - 1,
		"%s:%hu\tRPT\t%s", ip, port, server_role);
	memset(g_report_ctx.version, 0, sizeof(g_report_ctx.version));
	strncpy(g_report_ctx.version, version, sizeof(g_report_ctx.version) - 1);
	return 0;
}

void
report_write(const char *module, const char *format, ...)
{
	assert(NULL != format);
	time_t t;
	struct tm now_time;
	char buf[1024];
	va_list ap;

	t = time(NULL);
	if (NULL == localtime_r(&t, &now_time)) {
		return;
	}

	memset(buf, 0, sizeof(buf));
	int ret = snprintf(buf, sizeof(buf)-1,
		"%s\t%04d-%02d-%02d %02d:%02d:%02d\t%s\t%s\t",
		g_report_ctx.prefix_tags, now_time.tm_year + 1900,
		now_time.tm_mon + 1,
		now_time.tm_mday, now_time.tm_hour, now_time.tm_min,
		now_time.tm_sec, module, g_report_ctx.version);

	if (ret > 0) {
		va_start(ap, format);
		ret += vsnprintf(buf + ret, sizeof(buf)-ret - 1, format, ap);
		va_end(ap);
	}

	buf[sizeof(buf)-1] = '\n';
	if (ret > 0 && ret < (int)sizeof(buf)) {
		strcpy(buf + ret, "\n");
		ret += strlen("\n");
	}

	if (-1 != g_report_ctx.fd)
		ret = write(g_report_ctx.fd, buf, ret >(int)sizeof(buf) ? (int)sizeof(buf) : ret);
}

void
report_fini()
{
	if (-1 != g_report_ctx.fd)
		close(g_report_ctx.fd);
}
