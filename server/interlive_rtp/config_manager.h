/**
* @file  config_manager.h
* @brief Config manager
*
* @author houxiao
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>houxiao@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2015/06/23
*/

#ifndef _CONFIG_MANAGER_H_
#define _CONFIG_MANAGER_H_

#include "../util/xml.h"
#include <map>
#include <vector>
#include <string>
#include <stdint.h>

class FilePath
{
public:
    int32_t load_full_path(const char* path);

    std::string full_path;
    std::string dir;
    std::string file_name;
};

class ConfigModule
{
public:
    ConfigModule() {}
    virtual ~ConfigModule() {}
    virtual void set_default_config() = 0;
    virtual bool load_config(xmlnode* xml_config) = 0;
    virtual bool reload() const { return true; }
    virtual const char* module_name() const = 0;
    virtual void dump_config() const = 0;
};

typedef bool(*config_module_iter_func)(ConfigModule* config_module);

class ConfigManager
{
public:
    static ConfigManager& get_inst();
    static ConfigModule* get_inst_config_module(const char* name);

    ConfigManager();
    ~ConfigManager();

    void set_default_config();
    bool load_config(xmlnode* xml_config);
    bool load_config(const char* name, xmlnode* xml_config);
    bool reload() const;
    bool reload(const char* name) const;
    bool reload(ConfigManager& new_config_manager);
    ConfigModule* get_config_module(const char* name);
    ConfigModule* set_config_module(ConfigModule* config_module);
    void foreach_config_module(config_module_iter_func iter_func);
    void dump_config() const;


private:
    typedef std::map<std::string, ConfigModule*> cm_map_t;
    cm_map_t _name_config_modules;

    typedef std::vector<ConfigModule*> cm_vec_t;
    cm_vec_t _order_config_modules;
};

#endif
