#include <time.h>
#include "sample_app.hpp"



SampleApp::SampleApp(int argc, char** argv)
        :Application(argc, argv),
{
}
SampleApp::~SampleApp()
{
    if (_processor)
    {
        delete _processor;
        _processor = NULL;
    }

    if (_reactor)
    {
        delete _reactor;
        _reactor = NULL;
    }
}


int SampleApp::impl_init()
{
    int rv = 0;
    char* path_p;
    string root_path;

    if((path_p = getenv("MQ_HOME")))
    {
        root_path = path_p;
    }
    else
    {
        cout<<"get env MQ_HOME  failed "<<endl;
        return -1;
    }


    //
    rv = _config.init(root_path + "/conf/mqtcpd.conf");
    if(rv<0)
    {
        LOG4CPLUS_ERROR(g_logger, "config init failed ");
        return -1;
    }

    //processor init
    _processor = new DefaultMessageProcessor;
    rv = _processor->init();
    if(rv < 0)
    {
        LOG4CPLUS_ERROR(g_logger,"datagram proc init failed");
        return -1;
    }
    else
    {
        LOG4CPLUS_INFO(g_logger, "processor init success");
    }


    int server_port ;
    rv = _config.getIntParam("port", server_port);
    if(rv < 0)
    {
        LOG4CPLUS_ERROR(g_logger, "get config item [port] failed ");
        return -1;
    }
    else
    {
        LOG4CPLUS_INFO(g_logger, "config item [port]="<<server_port);
    }

    _reactor = new Reactor(server_port, *_processor);
    rv = _reactor->init();
    if(rv < 0)
    {
        LOG4CPLUS_ERROR(g_logger,"reactor init failed");
        return -1;
    }
    else
    {
        LOG4CPLUS_INFO(g_logger, "reactor init success");
    }


    //start components

    _processor->start();

    return 0;
}

void SampleApp::run()
{
    int rv_reactor = 0;
    int rv_proc = 0;

    rv_reactor = _reactor->poll();
    if(rv_reactor < 0)
    {
        set_stopped(true);
    }

    //timer queue check
    _timer_queue.expire(time(0));
}


int main(int argc, char* argv[])
{
    int rv = 0;
    SampleApp* tcpd = SampleApp::get_instance<SampleApp,int, char**>(argc,argv);
    rv = tcpd->init();

    if(rv ==  0)
    {
        LOG4CPLUS_INFO(g_logger, "tcpd init success");
        tcpd->start();
    }
    else
    {
        cout<<"tcpd init failed"<<endl;
    }

}
