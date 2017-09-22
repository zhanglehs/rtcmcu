/*
 * author: hechao@youku.com
 * create: 2014/3/17
 *
 */

#ifndef __HTTP_POSTABLE_H_
#define __HTTP_POSTABLE_H_ 
#include <utils/buffer.hpp>

class Postable
{
    public:
        virtual ~Postable() {};
        virtual int post(const char *req_path, const char *req_body, lcdn::base::Buffer *result) = 0;
};

#endif

