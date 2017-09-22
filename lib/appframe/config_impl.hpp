/**
 * \file 
 * \brief Config 内部实现
 */
#ifndef _CONFIG__IMPL_HPP_
#define _CONFIG__IMPL_HPP_

#include <string>
#include <map>
#include <vector>
#include <stdint.h>

using namespace std;


class LineReader
{
public:
    LineReader(fstream& in_stream)
            :_inlimit (0),
            _inoff(0),
            _in_stream(in_stream)
    {

    }
    int readline();


    char _inbuf[8192];
    char _linebuf[1024];
    int   _linelimit;
    int   _inlimit;
    int   _inoff;

    fstream& _in_stream;
};



class ConfigImpl
{
public:
    struct Line
    {
		Line()
		{
			changed = false;
			deleted = false;
		}

        enum
        {
            NORMAL_LINE = 0,
            COMMENT_LINE = 1,
            SECTION_LINE   = 2,
            BLANK_LINE       = 3,
        };

        bool       changed;
        bool       deleted;
        uint32_t  no;
        uint32_t  type;
        string      key;
        string      value;
        string      comment;
    };

    struct Section
    {
        string  name;
        string  comment;
        vector<Line>      lines;
        map<string, uint32_t>    params;
    };

    ConfigImpl();
    ~ConfigImpl();
    int load(string cfg_file);
    int store();
    int get_int_param(const string& section,const  string& param_name, int& value);
    int get_double_param(const string& section, const string& param_name, double& value);
    int get_string_param(const string& section, const string& param_name, string& value);

    int set_int_param(const string& section, const  string& param_name, int value);
    int set_double_param(const string& section, const string& param_name,  double value);
    int set_string_param(const string& section, const  string& param_name, string& value);
    int delete_param(const string& section, const string& param_name);

private:
    void reset();
    int do_load(fstream& in_stream);

    int parse_normal_line();
    int parse_section_line();
    int parse_comment_line();
    int parse_blank_line();
public:
    static const int MAX_SECTION_NUM = 50;
private:
    string _cfg_file;

    LineReader* _lr;
    int32_t       _current_section;

    vector<Section>  _sections;
    int32_t              _section_num;
};


#endif

