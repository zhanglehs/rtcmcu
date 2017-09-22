
#include "utility.hpp"
#include <sys/time.h>
#include <stdio.h>

namespace Utility
{
    using namespace std;

    /**
     * \see strftime
     */
    char * get_cur_time_formated(char * buf, size_t buf_size, const char * fmt)
    {
	struct timeval  tp;
	struct tm tf;

	if (-1 == gettimeofday(&tp, NULL))
	{
	    return "";
	}

	localtime_r(&(tp.tv_sec), &tf);

	strftime(buf, buf_size, fmt, &tf);
	return buf;
    }

    string get_standard_cur_timestr()
    {
	char buffer[32] = {0};
	return get_cur_time_formated(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S");
    }

    string get_cur_4y2m2d()
    {
	char buffer[20] = {0};
	return get_cur_time_formated(buffer, sizeof(buffer), "%Y%m%d");
    }

    string get_cur_4y2m2d2h2m()
    {
	char buffer[20] = {0};
	return get_cur_time_formated(buffer, sizeof(buffer), "%Y%m%d%H%M");
    }

    string get_cur_4y2m2d2h2m2s()
    {
	char buffer[20] = {0};
	return get_cur_time_formated(buffer, sizeof(buffer), "%Y%m%d%H%M%S");
    }
};

