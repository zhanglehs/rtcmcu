/*
 *
 */

#include "config_info.h"
#include <iostream>

using namespace std;

int ConfigInfo::load(Config &engine, const string config_file)
{
    // TODO 
    int rv = 0;

    rv = engine.load(config_file);
    if (rv < 0)
    {
        cerr << "load " << config_file << " error. " << endl;
        return -1;
    }

    rv = engine.get_string_param("log", "log_config_file", log_config_file);
    if (rv < 0)
    {
        cerr << "get log.log_config_file from " << config_file << " error. " << endl;
        return -1;
    }

    rv = engine.get_int_param("log", "log_level", log_level);
    if (rv < 0)
    {
        cerr << "get log.log_level from " << config_file << "error. " << endl;
        return -1;
    }

    rv = engine.get_string_param("log", "log_ffmpeg_path", log_ffmpeg_path);
    if (rv < 0)
    {
        cerr << "get log.log_ffmpeg_path from " << config_file << "error. " << endl;
        return -1;
    }

    rv = engine.get_string_param("admin_server", "listen_ip", admin_listen_ip);
    if (rv < 0)
    {
        cerr << "get admin_server.listen_ip from " << config_file << " error. " << endl;
        return -1;
    }

    int port;
    rv = engine.get_int_param("admin_server", "listen_port", port);
    if (rv < 0)
    {
        cerr << "get admin_server.listen_port from " << config_file << " error. " << endl;
        return -1;
    }
    admin_listen_port = port;

    int count;
    rv = engine.get_int_param("admin_server", "max_ants_count", count);
    if (rv < 0)
    {
        cerr << "get admin_server.max_ants_count from " << config_file << " error. " << endl;
        return -1;
    }
    admin_max_ants_count = count;

    int tolerate_failure_time;
    rv = engine.get_int_param("AntPair", "tolerate_failure_time", tolerate_failure_time);
    if (rv < 0)
    {
        cerr << "get ScheduledAntPair.tolerate_failure_time from " << config_file << " error. " << endl;
        return -1;
    }
    this->ant_tolerate_failure_time = tolerate_failure_time;

    int tolerate_failure_count;
    rv = engine.get_int_param("AntPair", "tolerate_failure_count", tolerate_failure_count);
    if (rv < 0)
    {
        cerr << "get ScheduledAntPair.tolerate_failure_count from " << config_file << " error. " << endl;
        return -1;
    }
    this->ant_tolerate_failure_count = tolerate_failure_count;

    return 0;
}

int ConfigInfo::load(const string config_file)
{
    Config config;
    return load(config, config_file);
}
