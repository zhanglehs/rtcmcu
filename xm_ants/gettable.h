/*
 * author: hechao@youku.com
 * create: 2014/3/17
 *
 */

#ifndef __HTTP_GETABLE_H_
#define __HTTP_GETABLE_H_

#include <utils/buffer.hpp>

class Getable
{
    public:
        virtual ~Getable() {};
        virtual int get(const char *req_path, lcdn::base::Buffer *result) = 0;
};

#endif

