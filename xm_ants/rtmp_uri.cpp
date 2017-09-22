/*
 * author: hechao@youku.com
 * create: 2014/3/19
 *
 */

#include <cstring>
#include "rtmp_uri.h"

int RTMPURI::parse(const std::string &rtmp_uri)
{
    URI::parse(rtmp_uri.c_str());

    const char * s = strchr(get_host().c_str(), '/');
    if (*++s == '\0')
    {
        return -1;
    }
    const char * e = strchr(s, '/');
    if (e == NULL)
    {
        _app_name = std::string(s);
    }
    else
    {
        _app_name = std::string(s, e-s);
    }
    return 0;

}

