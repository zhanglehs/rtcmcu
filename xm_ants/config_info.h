/*
 *
 *
 */

#ifndef __CONFIG_INFO_H_
#define __CONFIG_INFO_H_

#include <string>
#include <appframe/config.hpp>

class ConfigInfo
{
    public:
        ConfigInfo() {}
        int load(Config & engine, const std::string config_file);
        int load(const std::string config_file);

        std::string log_config_file;
        std::string log_ffmpeg_path;
        int log_level;
        std::string admin_listen_ip;
        short admin_listen_port;
        size_t admin_max_ants_count;

        int ant_tolerate_failure_time;
        int ant_tolerate_failure_count;
};

#endif


