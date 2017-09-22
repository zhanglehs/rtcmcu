/*
 * author: hechao@youku.com
 * create: 2014/3/20
 *
 */

#include "global_var.h"

uint64_t        g_current_time_ms = 0;
ConfigInfo      g_config_info;
Arguments       g_arguments;

struct event_base *g_event_base = event_base_new();

Logger g_logger         = Logger::getInstance("running");
Logger g_log_operation  = Logger::getInstance("operation");
Logger g_log_exception  = Logger::getInstance("exception");
Logger g_log_statInfo   = Logger::getInstance("statinfo");


