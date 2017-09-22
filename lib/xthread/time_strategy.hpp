#ifndef __TIMESTRATEGY_H__
#define __TIMESTRATEGY_H__

#include <sys/time.h>



/**
 * @class TimeStrategy
 *
 * Implement a strategy for time operatons based on gettimeofday
 */
class TimeStrategy
{

    struct timeval _value;

public:

    TimeStrategy() {
        gettimeofday(&_value, 0);
    }

    inline unsigned long seconds() const {
        return _value.tv_sec;
    }

    inline unsigned long milliseconds() const {
        return _value.tv_usec/1000;
    }

    unsigned long seconds(unsigned long s) {

        unsigned long z = seconds();
        _value.tv_sec = s;

        return z;

    }

    unsigned long milliseconds(unsigned long ms) {

        unsigned long z = milliseconds();
        _value.tv_usec = ms*1000;

        return z;

    }

};



#endif
