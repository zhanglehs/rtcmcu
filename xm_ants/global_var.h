#ifndef _GLOBAL_VAR_H_
#define _GLOBAL_VAR_H_

#include "logger.h"

#include "arguments.h"
#include "config_info.h"

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>
#include "levent.h"

using namespace log4cplus;

extern Logger g_logger;
extern Logger g_log_operation;
extern Logger g_log_exception;
extern Logger g_log_statInfo;

extern const char *g_version;
extern ConfigInfo g_config_info;
extern Arguments g_arguments;

extern struct event_base *g_event_base;

extern time_t process_start_time;

//typedef CircularArrayQueue<LogItem> LogQueue;
//extern uint64_t               g_current_time_ms;

#endif //_GLOBAL_VAR_H_


