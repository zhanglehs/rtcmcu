/**
 * \file
 * \brief 需要读取配置文件的代码，请包含此头文件
 */
#ifndef _CONFIG_HPP_
#define _CONFIG_HPP_

#include <string>

using namespace std;

class ConfigImpl;

class Config
{
public:
    Config();
    ~Config();
    int load(const string& cfg_file);
    int store();
    int get_int_param(const string& section, const string& param_name, int& value);
    int get_double_param(const string& section, const string& param_name,  double& value);
    int get_string_param(const string& section, const string& param_name, string& value);
	int get_time_param(const string & section,const string & param_name,int &second);
    int set_int_param(const string& section, const string& param_name, int value);
    int set_double_param(const string& section, const string& param_name,  double value);
    int set_string_param(const string& section, const string& param_name, string& value);
    int delete_param(const string& section, const string& param_name);

    int load(const char* cfg_file);
    int get_int_param(const char* section, const char* param_name, int& value);
    int get_double_param(const char* section, const char* param_name,  double& value);
    int get_string_param(const char* section, const char* param_name, string& value);
    int set_int_param(const char* section, const char* param_name, int value);
    int set_double_param(const char* section, const char* param_name,  double value);
    int set_string_param(const char* section, const char* param_name, string& value);
    int delete_param(const char* section, const char* param_name);

private:
    ConfigImpl* _impl;
};

#endif
