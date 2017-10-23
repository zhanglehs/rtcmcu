#ifndef _LCDN_BASE_LOGGING_H_
#define _LCDN_BASE_LOGGING_H_

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

extern bool g_log_initialized;
extern log4cplus::Logger g_logger_running;
void log_initialize_default();
void log_initialize_file();

namespace lcdn
{
namespace base
{

#define LOG_TRACE(b) do { \
    if (g_log_initialized) { LOG4CPLUS_TRACE(g_logger_running,b); } \
} while(0)

#define LOG_DEBUG(b) do { \
    if (g_log_initialized) { LOG4CPLUS_DEBUG(g_logger_running,b); } \
} while(0)

#define LOG_INFO(b) do { \
    if (g_log_initialized) { LOG4CPLUS_INFO(g_logger_running,b); } \
} while(0)

#define LOG_WARN(b) do { \
    if (g_log_initialized) { LOG4CPLUS_WARN(g_logger_running,b); } \
} while(0)

#define LOG_ERROR(b) do { \
    if (g_log_initialized) { LOG4CPLUS_ERROR(g_logger_running,b); } \
} while(0)

#define LOG_FATAL(b) do { \
    if (g_log_initialized) { LOG4CPLUS_FATAL(g_logger_running,b); } \
} while(0)

} // namespace base
} // namespace lcdn

#endif // _LCDN_BASE_LOGGING_H_
