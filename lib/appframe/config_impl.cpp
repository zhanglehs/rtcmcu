#include <cstring>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include "config_impl.hpp"

ConfigImpl::ConfigImpl()
{
    reset();
}

ConfigImpl::~ConfigImpl()
{
}

int ConfigImpl::get_int_param(const string& section,const  string&  param_name, int& value)
{
    int rv = 0;
    string tmp;

    rv = get_string_param(section, param_name, tmp);
    if(rv < 0)
    {
        return -1;
    }

    value = strtol(tmp.c_str(),0,0);

    return 0;
}

int ConfigImpl::get_double_param(const string& section, const string&  param_name, double& value)
{

    int rv = 0;
    string tmp;

    rv = get_string_param(section, param_name, tmp);
    if(rv < 0)
    {
        return -1;
    }

    value = atof(tmp.c_str());

    return 0;
}

int ConfigImpl::get_string_param(const string& section, const string&  param_name, string& value)
{
    string section_name = "globalsection";
    if(section != "")
    {
        section_name = section;
    }

    for(int i=0; i< _section_num; i++)
    {
        if(_sections[i].name == section_name)
        {
            map<string, uint32_t> ::iterator iter;
            iter =_sections[i].params.find(param_name);

            if(iter == _sections[i].params.end())
            {
                //cout<<"can not find config item\n";
                return -1;
            }
            else
            {
                value = _sections[i].lines[iter->second].value;
                return 0;
            }

        }
    }

    return -1;
}

int ConfigImpl::set_int_param(const string& section, const string& param_name, int value)
{
    stringstream ss;
    ss<<value;

    string converted = ss.str();

    return set_string_param(section, param_name, converted);
}

int ConfigImpl::set_double_param(const string& section, const string& param_name, double value)
{
    stringstream ss;
    ss<<value;

    string converted = ss.str();

    return set_string_param(section, param_name, converted);
}

int ConfigImpl::set_string_param(const string& section, const string& param_name, string& value)
{
    string section_name = "globalsection";
    
    if(section != "")
    {
        section_name = section;
    }

    for(int i=0; i< _section_num; i++)
    {
        if(_sections[i].name == section_name)
        {
            map<string, uint32_t> ::iterator iter;
            iter =_sections[i].params.find(param_name);

            if(iter == _sections[i].params.end())
            {
                return -1;
            }
            else
            {
                _sections[i].lines[iter->second].value = value;
				return 0;
            }
        }
    }

    return -1;
}

int ConfigImpl::delete_param(const string& section, const string& param_name)
{
    string section_name = "globalsection";
    
    if(section != "")
    {
        section_name = section;
    }

    for(int i=0; i< _section_num; i++)
    {
        if(_sections[i].name == section_name)
        {
            map<string, uint32_t> ::iterator iter;
            iter =_sections[i].params.find(param_name);

            if(iter == _sections[i].params.end())
            {
                return -1;
            }
            else
            {
                _sections[i].lines[iter->second].deleted = true;
                _sections[i].params.erase(iter);
            }

        }
    }

    return 0;
}

int ConfigImpl::load(string cfg_file)
{
    int rv = 0;

    reset();
    _cfg_file = cfg_file;

    fstream conf;

    conf.open(_cfg_file.c_str(), ios::in | ios::binary);
    if(!conf)
    {
        conf.close();
        return -1;
    }

    rv = do_load(conf);
    conf.close();

    return rv ;
}

int  ConfigImpl::do_load(fstream& in_stream)
{
    int rv = 0;
    char c = 0;
    int limit = 0;

    _lr = new LineReader(in_stream);

    while (((limit = _lr->readline()) >= 0)
            &&(rv == 0))
    {
        c = _lr->_linebuf[0];
        switch(c)
        {
        case '[':
            rv = parse_section_line();
            break;

        case '#':
        case '!':
            rv =parse_comment_line();
            break;

        default:
            rv = parse_normal_line();
            break;
        }
    }

    delete _lr;
    _lr = NULL;

    return rv;
}

int ConfigImpl::parse_normal_line()
{
    int limit = _lr->_linelimit;
    int key_len;
    int value_start;
    char c;
    bool has_sep;
    bool preceding_backslash;


    c = 0;
    key_len = 0;
    value_start = limit;
    has_sep = false;

    preceding_backslash = false;
    while (key_len < limit)
    {
        c = _lr->_linebuf[key_len];
        //need check if escaped.
        if ((c == '=' ||  c == ':') && !preceding_backslash)
        {
            value_start = key_len + 1;
            has_sep = true;
            break;
        }
        else if ((c == ' ' || c == '\t' ||  c == '\f') && !preceding_backslash)
        {
            value_start = key_len + 1;
            break;
        }
        if (c == '\\')
        {
            preceding_backslash = !preceding_backslash;
        }
        else
        {
            preceding_backslash = false;
        }
        key_len++;
    }

    while (value_start < limit)
    {
        c = _lr->_linebuf[value_start];
        if (c != ' ' && c != '\t' &&  c != '\f')
        {
            if (!has_sep && (c == '=' ||  c == ':'))
            {
                has_sep = true;
            }
            else
            {
                break;
            }
        }
        value_start++;
    }

	string key(&_lr->_linebuf[0], key_len);
	string value;
    if(limit - value_start > 0)
    {
        value = string(&_lr->_linebuf[value_start], limit - value_start);
	}
	else
	{
		value = string("");
	}

    Line line;
    line.type = Line::NORMAL_LINE;
    line.key  = key;
    line.value  = value;

    if(_current_section == -1)
    {
        _section_num++;
        _current_section = 0;
        _sections[_current_section].name="globalsection";
        _sections[_current_section].lines.push_back(line);
        line.no =  _sections[_current_section].lines.size()-1;
        _sections[_current_section].params.insert(map<string, uint32_t>::value_type(key, line.no));
    }
    else
    {
        _sections[_current_section].lines.push_back(line);
        line.no =  _sections[_current_section].lines.size()-1;
        _sections[_current_section].params.insert(map<string, uint32_t>::value_type(key, line.no));

    }

    return 0;
}

int ConfigImpl::parse_section_line()
{
    int section_start = 0, section_end=0, tmp_section_end=0;
    int comment_start = 0;
    int iterator = 0;
    char c;
    bool preceding_backslash;
    bool error_ocurred = false;

    c = 0;
    preceding_backslash = false;

    for (iterator = 0; iterator < _lr->_linelimit; iterator++)
    {
        c = _lr->_linebuf[iterator];

        if(c=='[')
        {
            if (iterator == 0)
            {
                continue;
            }
            else
            {
                error_ocurred  = true;
                break;
            }
        }

        if ((c == ' ' || c == '\t' ||  c == '\f'))
        {
            if(section_start == 0)
            {
                continue;
            }
            else if(tmp_section_end == 0)
            {
                tmp_section_end = iterator -1;
                continue;
            }
            else
            {

                continue;
            }
        }


        if(c == ']')
        {
            if(section_start > 0)
            {
                if(tmp_section_end==0)
                {
                    section_end = iterator -1;
                }
                else
                {
                    section_end = tmp_section_end;
                }

                continue;
            }
            else
            {
                error_ocurred  = true;
                break;
            }
        }

        if((c=='#') || (c=='!'))
        {
            if((section_end > 0) && (section_start > 0))
            {
                comment_start = iterator;
            }
            else
            {
                error_ocurred = true;
            }

            break;
        }

        if (c == '\\')
        {
            preceding_backslash = !preceding_backslash;
        }
        else
        {
            preceding_backslash = false;
        }


        if(section_start == 0)
        {
            section_start = iterator;
        }
        else if(tmp_section_end !=0)
        {
            error_ocurred = true;
            break;
        }

    }

    if(!error_ocurred)
    {
        if((section_start > 0) && (section_end > 0))
        {

            string name(&_lr->_linebuf[section_start], section_end - section_start +1);

            if(_current_section == -1)
            {
                _current_section = 0;
            }
            else
            {
                _current_section++;
            }

            _sections[_current_section].name = name;

            if(comment_start != 0)
            {
                string comment(&_lr->_linebuf[comment_start], _lr->_linelimit - comment_start);
                _sections[_current_section].comment = comment;
            }

            _section_num++;

            return 0;

        }
        else
        {
            return -1;
        }

    }
    else
    {
        return -1;
    }

}

int ConfigImpl::parse_comment_line()
{
    Line line;
    string comment_line(&_lr->_linebuf[0], _lr->_linelimit);

    line.type = Line::COMMENT_LINE;
    line.comment = comment_line;

    _sections[_current_section].lines.push_back(line);

    return 0;
}

int ConfigImpl::parse_blank_line()
{
    Line line;
    line.type = Line::BLANK_LINE;
    _sections[_current_section].lines.push_back(line);

    return 0;
}

int ConfigImpl::store()
{
    fstream conf;

    conf.open(_cfg_file.c_str(), ios::out | ios::binary);
    if(!conf)
    {
        return -1;
    }

    for(int i=0; i < _section_num; i++)
    {
        if(_sections[i].name != "globalsection")
        {
            conf<<"["<<_sections[i].name<<"]";
            if(_sections[i].comment != "")
            {
                conf<<"\t\t\t"<< _sections[i].comment;
            }
            conf<<endl;
        }

        vector<Line>::iterator iter = _sections[i].lines.begin();
        for(; iter != _sections[i].lines.end(); iter++)
        {
            if(iter->deleted)
            {
                continue;
            }
            
            conf<<iter->key<<"="<<iter->value;
            if(iter->comment !="")
            {
                conf<<"\t\t\t"<<iter->comment;
            }
            conf<<endl;
        }

         conf<<endl<<endl;
    }

    conf.close();
    
    return 0;

}


void ConfigImpl::reset()
{
    _lr = NULL;
    _current_section=-1;
	_sections.clear();
    _sections.resize(MAX_SECTION_NUM);
    _section_num = 0;
    _cfg_file = "";
}

/* read in a "logical line" from input stream, skip all comment and
 * blank lines and filter out those leading whitespace characters
 * (\u0020, \u0009 and \u000c) from the beginning of a "natural line".
 * Method returns the char length of the "logical line" and stores
 * the line in "_linebuf".
 */


int LineReader::readline()
{
    char c = 0;

    bool skip_whitespace = true;
    bool is_commentline = false;
    bool is_newline = true;
    bool appended_linebegin = false;
    bool preceding_backslash = false;
    bool skip_LF = false;

    _linelimit = 0;

    while (true)
    {
        if (_inoff >= _inlimit)
        {
            _in_stream.read(_inbuf, 8192);
            _inlimit = _in_stream.gcount();
            _inoff = 0;
            if (_inlimit <= 0)
            {
                if (_linelimit == 0 || is_commentline)
                {
                    return -1;
                }
                return _linelimit;
            }
        }

        //The line below is equivalent to calling a
        //ISO8859-1 decoder.
        c = (char) (0xff & _inbuf[_inoff++]);
        if (skip_LF)
        {
            skip_LF = false;
            if (c == '\n')
            {
                continue;
            }
        }

        if (skip_whitespace)
        {
            if (c == ' ' || c == '\t' || c == '\f')
            {
                continue;
            }
            if (!appended_linebegin && (c == '\r' || c == '\n'))
            {
                continue;
            }
            skip_whitespace = false;
            appended_linebegin = false;
        }
        if (is_newline)
        {
            is_newline = false;
            if (c == '#' || c == '!')
            {
                is_commentline = true;
                continue;
            }
        }

        if (c != '\n' && c != '\r')
        {
            _linebuf[_linelimit++] = c;
            if (_linelimit == 1024)
            {
                return -1;
            }
            //flip the preceding backslash flag
            if (c == '\\')
            {
                preceding_backslash = !preceding_backslash;
            }
            else
            {
                preceding_backslash = false;
            }
        }
        else
        {
            // reached EOL
            if (is_commentline || _linelimit == 0)
            {
                is_commentline = false;
                is_newline = true;
                skip_whitespace = true;
                _linelimit = 0;
                continue;
            }
            if (_inoff >= _inlimit)
            {
                _in_stream.read(_inbuf, 8192);
                _inlimit = _in_stream.gcount();
                _inoff = 0;

                if (_inlimit <= 0)
                {
                    return _linelimit;
                }
            }
            if (preceding_backslash)
            {
                _linelimit -= 1;
                //skip the leading whitespace characters in following line
                skip_whitespace = true;
                appended_linebegin = true;
                preceding_backslash = false;
                if (c == '\r')
                {
                    skip_LF = true;
                }
            }
            else
            {
                return _linelimit;
            }
        }
    }
}

