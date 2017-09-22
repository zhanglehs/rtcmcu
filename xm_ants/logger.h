/********************************************************************
    description:    Switch of logger
*********************************************************************/
#ifndef SANDAI_C_LOGGER_H_200508162107
#define SANDAI_C_LOGGER_H_200508162107

#define LOG_TRACE(a,b) LOG4CPLUS_TRACE(a,b)
#define LOG_DEBUG(a,b) LOG4CPLUS_DEBUG(a,b)
#define LOG_INFO(a,b)  LOG4CPLUS_INFO(a,b)
#define LOG_WARN(a,b)  LOG4CPLUS_WARN(a,b)
#define LOG_ERROR(a,b) LOG4CPLUS_ERROR(a,b)
#define LOG_FATAL(a,b) LOG4CPLUS_FATAL(a,b)

#ifdef LOGGER
#include <log4cplus/logger.h>

#define DECL_LOGGER(logger) static log4cplus::Logger logger;
#define IMPL_LOGGER(classname, logger) log4cplus::Logger classname::logger = log4cplus::Logger::getInstance(#classname);
#define TEMPLATE_IMPL_LOGGER(classname, logger) template<class T> log4cplus::Logger classname<T>::logger = log4cplus::Logger::getInstance(#classname);

#else
#define _LOG4CPLUS_LOGGING_MACROS_HEADER_
#define LOG4CPLUS_TRACE(a,b)
#define LOG4CPLUS_DEBUG(a,b)
#define LOG4CPLUS_INFO(a,b)
#define LOG4CPLUS_WARN(a,b)
#define LOG4CPLUS_ERROR(a,b)
#define LOG4CPLUS_FATAL(a,b)

#define DECL_LOGGER(logger)
#define IMPL_LOGGER(classname, logger)
#define TEMPLATE_IMPL_LOGGER(classname, logger)

#endif

#endif
