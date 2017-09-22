/*
 * author: hechao@youku.com
 * create: 2014/3/20
 *
 */
#include <string>

#include "xmants.h"
#include "global_var.h"
#include "admin_server.h"
#include "arguments.h"
#include <net/net_utils.h>

using namespace std;

XMAnts::XMAnts(string config_file_name)
:_config_file_name(config_file_name), _event_base(NULL), _admin_server(NULL)
{

}
XMAnts::~XMAnts()
{
    LOG_INFO(g_logger, "XMAnts::~XMAnts");

    if (_log4cplus_property_configurator)
    {
        delete _log4cplus_property_configurator;
        _log4cplus_property_configurator = NULL;
    }

    if (_admin_server)
    {
        delete _admin_server;
        _admin_server = NULL;
    }
}

int XMAnts::init()
{
    if (g_config_info.load(this->_config_file_name) < 0)
      {
        std::cerr << "load config-file from arguments error. " << endl;
        LOG_FATAL(g_logger, "load config-file from arguments error. ");
        return -1;
    }

    // 3, init log
    string log_config_file = g_config_info.log_config_file;
    _log4cplus_property_configurator = new PropertyConfigurator(log_config_file);
    _log4cplus_property_configurator->configure();

    uint32_t ip;
    if (net_util_str2ip(g_config_info.admin_listen_ip.c_str(), &ip) != 0)
    {
        LOG_INFO(g_logger, "convert g_config_info.admin_listen_ip to int error.");
        return -1;
    }

    AntPair::set_config(g_config_info.ant_tolerate_failure_time, g_config_info.ant_tolerate_failure_count);

    _admin_server = new AdminServer(ip,
            g_config_info.admin_listen_port,
            g_config_info.admin_max_ants_count);

    return 0;
}

int XMAnts::run()
{
    LOG_INFO(g_logger, "XMAnts::start");

    _admin_server->set_event_base(_event_base);
    if (_admin_server->init() != 0)
    {
        LOG_INFO(g_logger, "init AdminServer failed");
        return -1;
    }

    return 0;
}

int XMAnts::start()
{
    return run();
}

int XMAnts::force_exit_all()
{
    // exit admin_server
    int exit_all = this->_admin_server->force_exit_all();

    // TODO: exit others
    return exit_all;
}
