/*
 * author: hechao@youku.com
 * create: 2014/3/19
 *
 */

#ifndef __RTMP_URI_H_
#define __RTMP_URI_H_

#include <net/uri.h>
#include <string>

class RTMPURI: public URI
{
    public:
        virtual ~RTMPURI(){}
        int parse(const std::string &rtmp_uri);
    private:
        std::string _app_name;
};
#endif

