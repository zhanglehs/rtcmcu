#include "access.h"


access_config g_conf_access;

static int
	print_prefix(char *buf, size_t sz, struct tm *now_time)
{
	return snprintf(buf, sz, "%02d:%02d:%02d ",
		now_time->tm_hour, now_time->tm_min, now_time->tm_sec);
}

static void
	access_write(struct tm *now_time, const char *format, ...)
{
	char buf[1024 * 8];
	va_list ap;

	buf[sizeof(buf)-1] = '\0';
	int ret = print_prefix(buf, sizeof(buf)-1, now_time);

	if (ret > 0) {
		va_start(ap, format);
		ret += vsnprintf(buf + ret, sizeof(buf)-ret - 1, format, ap);
		va_end(ap);
	}

	if (ret > 0 && ret < (int)sizeof(buf)) {
		strcpy(buf + ret, "\n");
		ret += strlen("\n");
	}

	if (-1 != g_conf_access.access_fd)
		ret = write(g_conf_access.access_fd, buf, ret);
}

//ret -10~-20
static int
	access_reopen_file(void)
{
	assert(ACCESS_MODE_FILE == g_conf_access.mode);
	int fd = -1;
	time_t t = 0;
	struct tm curr_tm;
	char file_path[sizeof(g_conf_access.file_path)];

	t = time(NULL);
	if (NULL == localtime_r(&t, &curr_tm))
		return -1;

	snprintf(file_path, sizeof(file_path)-1, "%s-%04d%02d%02d-%02d%02d%02d",
		g_conf_access.base_path,
		curr_tm.tm_year + 1900, curr_tm.tm_mon + 1, curr_tm.tm_mday,
		curr_tm.tm_hour, curr_tm.tm_min, curr_tm.tm_sec);
	file_path[sizeof(file_path)-1] = '\0';

	fd = open(file_path, O_CREAT | O_APPEND | O_WRONLY, 0644);
	if (-1 == fd) {
		if (-1 != g_conf_access.access_fd) {
			access_write(&curr_tm,
				"ERROR open failed. base_path = %s, file_path = %s",
				g_conf_access.base_path, file_path);
		}
		else {
			fprintf(stderr,
				"ERROR open failed. base_path = %s, file_path = %s\n",
				g_conf_access.base_path, file_path);
		}
		return errno;
	}

	fcntl(fd, F_SETFD, FD_CLOEXEC);

	if (-1 != g_conf_access.access_fd)
		close(g_conf_access.access_fd);
	g_conf_access.access_fd = fd;
	g_conf_access.last_day = curr_tm;
	strncpy(g_conf_access.file_path, file_path, sizeof(g_conf_access.file_path));

	return 0;
}

static int
	sig_safe_reopen()
{
	assert(ACCESS_MODE_FILE == g_conf_access.mode);
	int ret;
	sigset_t this_set;
	sigset_t old_set;
	sigset_t *pold = NULL;

	sigfillset(&this_set);
	sigemptyset(&old_set);
	if (0 == sigprocmask(SIG_SETMASK, &this_set, &old_set))
		pold = &old_set;

	ret = access_reopen_file();
	if (NULL != pold)
		sigprocmask(SIG_SETMASK, pold, NULL);

	return ret;
}

int
	access_init_file(const char *basepath, const char *name_prefix,
	size_t max_size)
{
	if (NULL == basepath
		|| strlen(basepath) >= PATH_MAX - 32 - strlen("-20130515-000000")
		|| strlen(name_prefix) > 32)
		return -1;

	char realbasepath[PATH_MAX];

	memset(realbasepath, 0, sizeof(realbasepath));
	memset(&g_conf_access, 0, sizeof(g_conf_access));
	g_conf_access.access_fd = -1;
	g_conf_access.max_size = max_size;
	g_conf_access.mode = ACCESS_MODE_FILE;

	if (NULL == realpath(basepath, realbasepath)) {
		fprintf(stderr, "realpath failed.\n");
		return errno;
	}

	snprintf(g_conf_access.base_path, sizeof(g_conf_access.base_path) - 1, "%s/%s",
		realbasepath, name_prefix);
	return access_reopen_file();
}

int
	access_init_net(const char *remotepath, const char *server_role,
	const char *ip, uint16_t port)
{
	memset(&g_conf_access, 0, sizeof(g_conf_access));
	g_conf_access.access_fd = -1;
	g_conf_access.mode = ACCESS_MODE_NET;
	g_conf_access.access_fd = open_statfile_or_pipe(remotepath);
	if (g_conf_access.access_fd < 0) {
		fprintf(stderr, "open_statfile_or_pipe failed. error = %s\n",
			strerror(errno));
		return -1;
	}
	snprintf(g_conf_access.prefix_tags, sizeof(g_conf_access.prefix_tags) - 1,
		"%s:%hu\tACCESS\t%s", ip, port, server_role);

	return 0;
}

static void
	check_reopen(struct tm *now_time)
{
	assert(ACCESS_MODE_FILE == g_conf_access.mode);
	int ret = 0;
	struct stat buf;
	int s_boolean = FALSE;
	int t_boolean = now_time->tm_mday != g_conf_access.last_day.tm_mday ||
		now_time->tm_mon != g_conf_access.last_day.tm_mon ||
		now_time->tm_year != g_conf_access.last_day.tm_year;
	if (!t_boolean) {
		ret = fstat(g_conf_access.access_fd, &buf);
		if (0 != ret) {
			access_write(now_time,
				"check reopen fstat failed. error = %s",
				strerror(errno));
			return;
		}
		s_boolean = (int)(buf.st_size >= (int)g_conf_access.max_size);
	}
	if (t_boolean || s_boolean) {
		ret = sig_safe_reopen();
	}

	if (ret < 0) {
		access_write(now_time, "sig_safe_reopen failed. ret = %d", ret);
	}
	else if (ret > 0) {
		access_write(now_time, "sig_safe_reopen failed. error = %s",
			strerror(ret));
	}
}

void
	access_print(const char *format, ...)
{
	time_t t;
	struct tm now_time;
	char buf[1024 * 8];
	va_list ap;

	if (NULL == format)
		return;

	t = time(NULL);
	if (NULL == localtime_r(&t, &now_time))
		return;
	int ret = 0;

	buf[sizeof(buf)-1] = '\0';
	if (ACCESS_MODE_FILE == g_conf_access.mode) {
		check_reopen(&now_time);
		// ret = print_prefix(buf, sizeof(buf)-1, &now_time);
		ret =
			snprintf(buf, sizeof(buf)-1,
			"%s\t%04d-%02d-%02d %02d:%02d:%02d\t",
			g_conf_access.prefix_tags, now_time.tm_year + 1900,
			now_time.tm_mon + 1,
			now_time.tm_mday, now_time.tm_hour, now_time.tm_min,
			now_time.tm_sec);

	}
	else if (ACCESS_MODE_NET == g_conf_access.mode) {
		ret =
			snprintf(buf, sizeof(buf)-1,
			"%s\t%04d-%02d-%02d %02d:%02d:%02d\t",
			g_conf_access.prefix_tags, now_time.tm_year + 1900,
			now_time.tm_mon + 1,
			now_time.tm_mday, now_time.tm_hour, now_time.tm_min,
			now_time.tm_sec);
	}

	if (ret > 0) {
		va_start(ap, format);
		ret += vsnprintf(buf + ret, sizeof(buf)-ret - 1, format, ap);
		va_end(ap);
	}

	if (ACCESS_MODE_FILE == g_conf_access.mode) {
		if (ret > 0 && ret < (int)sizeof(buf)) {
			strcpy(buf + ret, "\n");
			ret += strlen("\n");
		}
	}
	else if (ACCESS_MODE_NET == g_conf_access.mode) {
		if (ret > 0 && ret < (int)sizeof(buf)) {
			ret = ret > 1024 ? 1024 : ret;
			buf[ret++] = '\n';
		}
		else {
			return;
		}
	}

	if (-1 != g_conf_access.access_fd)
		ret = write(g_conf_access.access_fd, buf, ret);
}

void
	access_set_max_size(size_t max_size)
{
	g_conf_access.max_size = max_size;
}

void
	access_fini()
{
	if (-1 != g_conf_access.access_fd)
		close(g_conf_access.access_fd);
}