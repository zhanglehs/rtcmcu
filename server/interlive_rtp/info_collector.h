/**
* @file  info_collector.h
* @brief InfoCollector
*
* @author houxiao
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>houxiao@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2015/07/09
*/

#ifndef _INFO_COLLECTOR_H_
#define _INFO_COLLECTOR_H_

#include <ctime>

#ifndef nullptr
#define nullptr NULL
#endif

class InfoCollector
{
    friend class MTClientInfoCollector;

public:
    static InfoCollector& get_inst();

    InfoCollector();
    ~InfoCollector();

    void on_second(time_t t);

private:
    bool _0s_collected;
    bool _0s_sent;
    size_t total_collect;
    size_t total_send;

    void collect_info_0s();
    void collect_info_10s();

    void send_info_0s();
    void send_info_10s();
};

#endif
