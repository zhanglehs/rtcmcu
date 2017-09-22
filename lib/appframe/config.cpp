#include <stdlib.h>
#include <cstring>

#include "config.hpp"
#include "config_impl.hpp"

Config::Config()
        : _impl(new ConfigImpl)
{

}

Config::~Config()
{
    if(_impl)
    {
        delete _impl;
        _impl = NULL;
    }
}

int Config::load(const string& cfg_file)
{
    return _impl->load(cfg_file);
}

int Config::store()
{
    return _impl->store();
}


int Config::get_int_param(const string& section, const string& param_name, int& value)
{
    return _impl->get_int_param(section, param_name, value);
}

int Config::get_double_param(const string& section, const string& param_name, double& value)
{
    return _impl->get_double_param(section, param_name, value);
}

int Config::get_string_param(const string& section, const string& param_name, string& value)
{
    return _impl->get_string_param(section, param_name, value);
}
/*
*parse 30h,30m,30s to seconds
*/
int Config::get_time_param(const string & section,const string & param_name,int &second)	
{
	string v;
	if(get_string_param(section,param_name,v)<0)
		return -1;
	char s[256];
	strcpy(s,v.c_str());
	if((s[v.size()-1]=='s') || (s[v.size()-1]=='S'))
	{
		s[v.size()-1]='\0';
		second=atoi(s);
	}else if((s[v.size()-1]=='m') || (s[v.size()-1]=='M'))
	{		
		s[v.size()-1]='\0';
		second=atoi(s)*60;
	}
	else if((s[v.size()-1]=='h') || (s[v.size()-1]=='H'))
	{		
		s[v.size()-1]='\0';
		second=atoi(s)*3600;
	}else
		return -1;
	if(second<0)
		return -1;
	return 0;
}

int Config::set_int_param(const string& section, const string& param_name, int value)
{
    return _impl->set_int_param(section, param_name, value);
}

int Config::set_double_param(const string& section, const string& param_name, double value)
{
    return _impl->set_double_param(section, param_name, value);
}

int Config::set_string_param(const string& section, const string& param_name, string& value)
{
    return _impl->set_string_param(section, param_name, value);
}

int Config::delete_param(const string& section, const string& param_name)
{
    return _impl->delete_param(section, param_name);
}

int Config::load(const char* cfg_file)
{
	return load(string(cfg_file));
}

int Config::get_int_param(const char* section, const char* param_name, int& value)
{
	return get_int_param(string(section), string(param_name), value);
}

int Config::get_double_param(const char* section, const char* param_name,  double& value)
{
	return get_double_param(string(section), string(param_name), value);
}

int Config::get_string_param(const char* section, const char* param_name, string& value)
{
	return get_string_param(string(section), string(param_name), value);
}

int Config::set_int_param(const char* section, const char* param_name, int value)
{
	return set_int_param(string(section), string(param_name), value);
}

int Config::set_double_param(const char* section, const char* param_name,  double value)
{
	return set_double_param(string(section), string(param_name), value);
}

int Config::set_string_param(const char* section, const char* param_name, string& value)
{
	return set_string_param(string(section), string(param_name), value);
}

int Config::delete_param(const char* section, const char* param_name)
{
	return delete_param(string(section), string(param_name));
}
