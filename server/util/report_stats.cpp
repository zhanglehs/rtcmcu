#include "report_stats.h"

#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include "statfile.h"
#include "log.h"


ReportStats* ReportStats::_instance = NULL;
extern log_config g_log_ctx;

ReportStats::ReportStats()
{
    _fd = -1;
    _prefix_tags = "";
    _version = "";
    _process_name = "";
}


ReportStats::~ReportStats()
{
    fini();
}

ReportStats* ReportStats::get_instance()
{
    if (_instance == NULL)
    {
        _instance = new ReportStats();
    }
    return _instance;
}

int ReportStats::init(const char *path, const char *process_name, const char *version,
        const char *ip, uint16_t port)
{

    _fd = open_statfile_or_pipe(path);
    if (_fd < 0) {
        ERR("open stat file failed.");
        return -1;
    }
    _path = path;

    char temp[1024] = { 0 };
    snprintf(temp, sizeof(temp)-1, "%s:%hu\tRPT\t%s", ip, port, process_name);
    _prefix_tags = temp;

    memset(temp, 0, sizeof(temp));
    strncpy(temp, version, sizeof(temp)-1);
    _version = temp;

    _process_name = process_name;

    return 0;
}

void ReportStats::fini()
{
    if (-1 != _fd)
    {
        close(_fd);
        _fd = -1;
    }
}

void ReportStats::check_reopen()
{
    if (LOG_MODE_NET == g_log_ctx.mode )
    {
        return;
    }

    if ((access(_path.c_str(),F_OK) ) == -1)
    {
        DBG("ReportStats::check_reopen: %s not exist", _path.c_str());
        fini();

        _fd = open_statfile_or_pipe(_path.c_str());
        if (_fd < 0) {
            ERR("ReportStats::check_reopen: open stat file failed.");
            return;
        }
    }
}

void ReportStats::write(const char* module_name_str, int32_t report_type_id, \
    const char* report_type_name, \
    /*int32_t range_begin, int32_t range_end,*/ \
    const char *format, ...)
{
    assert(NULL != format || strlen(module_name_str) <= 0);

    if (strlen(module_name_str) <= 0)
    {
        ERR("write_ex failed. module_name_str is empty.");
        return;
    }

    /*if (report_type_id < range_begin || report_type_id > range_end)
    {
        ERR("write failed. report type is out of range:[%d,%d] report_type_id:%d",
            range_begin, range_end, report_type_id);
        return;
    }*/

    check_reopen();
    time_t t;
    struct tm now_time;
    char buf[1024];
    va_list ap;

    t = time(NULL);
    if (NULL == localtime_r(&t, &now_time))
    {
        ERR("write failed. localtime_r = NULL");
        return;
    }

    memset(buf, 0, sizeof(buf));
    int ret = snprintf(buf, sizeof(buf)-1,
        "%s\t%04d-%02d-%02d %02d:%02d:%02d\t%s\t%s\t%d\t%s",
        _prefix_tags.c_str(), now_time.tm_year + 1900,
        now_time.tm_mon + 1,
        now_time.tm_mday, now_time.tm_hour, now_time.tm_min,
        now_time.tm_sec, module_name_str, _version.c_str(), report_type_id, report_type_name);

    if (ret > 0)
    {
        va_start(ap, format);
        ret += vsnprintf(buf + ret, sizeof(buf)-ret - 1, format, ap);
        va_end(ap);
    }

    buf[sizeof(buf)-1] = '\n';
    if (ret > 0 && ret < (int)sizeof(buf))
    {
        strcpy(buf + ret, "\n");
        ret += strlen("\n");
    }
    /*DBG("ReportStats::_write %s", buf);
    if (-1 != _fd)
    {
        ::write(_fd, buf, ret >(int)sizeof(buf) ? (int)sizeof(buf) : ret);
    }*/
    ::write(g_log_ctx.log_fd, buf, ret >(int)sizeof(buf) ? (int)sizeof(buf) : ret);
}
