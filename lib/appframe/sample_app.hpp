/**
 * \file
 * \brief 使用Application的一个范例
 *
 * \note 应该移到appframe文件夹外边
 */
#ifndef _MQ_TCPD_HPP_
#define _MQ_TCPD_HPP_
#include "common.hpp"
#include "application.hpp"
#include "timer.hpp"
#include "reactor.hpp"
#include "default_message_processor.hpp"



class SampleApp : public Application
{
public:

    SampleApp(int argc, char* argv[]);
    ~SampleApp();

    timer_queue& get_timer_queue();
    void handle_log4cplus_event(void* param);
protected:
    int impl_start();
    int impl_init();
    void run();

private:
    DefaultMessageProcessor*  _processor;
    Reactor*                           _reactor;
    timer_queue                      _timer_queue;
};



#endif

