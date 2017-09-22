#include "logging.h"
#include  <log4cplus/layout.h>  
#include <log4cplus/consoleappender.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/configurator.h>
#include <cstring>
#include <string>
#include <unistd.h>

using namespace std;
using namespace lcdn;
using namespace lcdn::base;
using namespace log4cplus;

bool g_log_initialized = false;
Logger g_logger_running = Logger::getInstance("running");

int util_get_exe_absolute_path(char *full_path, int len)
{
    int i = 0;

    if (NULL == full_path)
    {
        fprintf(stderr, "util_get_exe_absolute_path:full_path is null. \n");
        return -1;
    }

    int cnt = readlink("/proc/self/exe", full_path, len);
    if (cnt < 0 || cnt >= len)
    {
        fprintf(stderr, "util_get_exe_absolute_path:resolve binary file path failed. \n");
        return -1;
    }

    full_path[cnt] = '\0';
    for (i=cnt; i>=0; i--)
    {
        if (full_path[i] == '/')
        {
            full_path[++i] = '\0';
            break;
        }

    }

    return i;
}




void log_initialize_default()
{
    if (!g_log_initialized)
    {
        SharedAppenderPtr append(new ConsoleAppender());
        append->setName("append test");
        string pattern = "%D{%m/%d/%y %H:%M:%S}  - %m [%l]%n";
        auto_ptr<Layout> layout(new PatternLayout(pattern));
        append->setLayout(layout);
        g_logger_running.addAppender(append);
        g_log_initialized = true;
    }
}


void log_initialize_file()
{
    if (!g_log_initialized)
    {
        /*SharedAppenderPtr append(new DailyRollingFileAppender("lreactor.log", DAILY, true, 7));//  FileAppender("lreactor.log"));
        append->setName("append test");
        string pattern = "%D{%m/%d/%y %H:%M:%S}  - %m [%l]%n";
        auto_ptr<Layout> layout(new PatternLayout(pattern));
        append->setLayout(layout);
        g_logger_running.addAppender(append);
        g_logger_running.setLogLevel(ALL_LOG_LEVEL);
        g_log_initialized = true;
        */

        char absolute_path[1024];
		memset(absolute_path, 0, sizeof(absolute_path));
        int str_len = 0;
        const char *cfg_file_name = "log4cplus.cfg";
        str_len = util_get_exe_absolute_path(absolute_path, 1024);
        strncpy(absolute_path+str_len, cfg_file_name, strlen(cfg_file_name));
        absolute_path[str_len+strlen(cfg_file_name)] = '\0';

        PropertyConfigurator::doConfigure(absolute_path);

        g_log_initialized = true;
    }
}
