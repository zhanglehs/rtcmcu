/*
 * author: hechao@youku.com
 * create: 2014/3/20
 *
 */
#include <string>
#include <map>
#include <iostream>
#include <appframe/config.hpp>

#include <log4cplus/configurator.h>
#include "admin_server.h"


using namespace log4cplus;
class Application
{
};

class XMAnts : public Application
{
    public:
        XMAnts(string config_file_name);
        ~XMAnts();
        void set_version(std::string ver) { _version = ver;}
        std::string get_version() { return _version;}
        int init();

        int start();
        int run();

        void set_event_base(struct event_base * base)
        {
            _event_base = base;
        }

        int force_exit_all();

    // TODO
    private:
        string _config_file_name;
        std::string _version;
        PropertyConfigurator *_log4cplus_property_configurator;
        struct event_base *_event_base;
        AdminServer *_admin_server;
    //
};

