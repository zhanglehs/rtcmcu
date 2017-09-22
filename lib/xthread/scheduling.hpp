#ifndef __SCHEDULING_H__
#define __SCHEDULING_H__

#include <algorithm>
#include <functional>
#include <deque>
#include <utility>

#include "thread.hpp"

/**
 * @author Eric Crahen <http://www.code-foo.com>
 * @date <2003-07-16T20:01:18-0400>
 * @version 2.2.0
 * @class fifo_list
 */
class fifo_list : public std::deque<Thread*>
{
public:

    void insert(const value_type& val) { push_back(val); }


};

/**
 * @author Eric Crahen <http://www.code-foo.com>
 * @date <2003-07-16T20:01:18-0400>
 * @version 2.2.0
 * @struct priority_order
 */
struct priority_order : public std::binary_function<Thread*, Thread*, bool> {

    std::less<const Thread*> id;

    bool operator()(const Thread* t0, const Thread* t1) const {

        if(t0->get_priority() > t1->get_priority())
            return true;

        else if (t0->get_priority() < t1->get_priority())
            return false;

        return id(t0, t1);

    }

};


/**
 * @author Eric Crahen <http://www.code-foo.com>
 * @date <2003-07-16T20:01:18-0400>
 * @version 2.2.0
 * @class priority_list
 */
class priority_list : public std::deque<Thread*>
{

    priority_order comp;

public:

    void insert(const value_type& val) {

        push_back(val);
        std::sort(begin(), end(), comp);

    }

};

#endif // __ZTSCHEDULING_H__
