/*
 * author: hechao@youku.com
 * create: 2014/3/18
 *
 */

#include "uri.h"

#include <cstring>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <assert.h>
#include <ctype.h>

using namespace std;

inline char toHex(char x)
{
    return x > 9 ? x -10 + 'A': x + '0';
}

inline char fromHex(char x)
{
    return isdigit(x) ? x-'0' : x-'A'+10;
}

URI::URI()
{
}

int URI::parse(const string& uri)
{
    return parse(URI::uri_decode(uri).c_str());
}

int URI::parse(const char *s)
{
    // parse protocol
    const char *p = strstr(s, "://");
    if (p != NULL)
    {
        _protocol = string(s, p-s);
    }

    s = p + 3;
    if (*s == '\0')
    {
        return -1;
    }

    // parse host and port 
    p = strchr(s, '/');
    if (p != NULL)
    {
        _parse_host_port(s,p);

        s = p;
        if (*s != '\0')
        {
            return parse_path(s);
        }
        else
        {
            return 0;
        }
    }
    else
    {
        _parse_host_port(s, NULL);
    }

    return 0;
}

int URI::parse_path(const char *path)
{
    return parse_path(string(path));
}

int URI::parse_path(const string &path)
{
	int pos = path.find(' ');
	string new_path;
	if (pos > 0)
	{
		new_path = path.substr(0, pos);
	}
	else
	{
		new_path = path;
	}

	const char * s = new_path.c_str();
    assert (s && *s == '/');

    const char * p = s;

    const char * ls = NULL; //last_slash = NULL;
    const char * qm = NULL; //quest_mark = NULL;

    while (*p)
    {
        if (*p == '?')
        {
            qm = p;
            break;
        }
        if (*p == '/')
        {
            ls = p;
        }

        p++;
    }

    if (ls && qm)
    {
        _file = string(ls+1, qm-ls-1);
        _path = string(s, ls+1-s);
    }
    else if (ls)
    {
        _file = string(ls+1);
        _path = string(s, ls+1-s);
    }
    else if (qm)
    {
        _file = string(s, qm-s);
        _path = '/';
    }
    else
    {
        _file = string(s);
        _path = '/';
    }

    if (*++p != '\0')
    {
        _parse_queries(p);
    }

    return 0;
}

void URI::_parse_host_port(const char *start, const char *end)
{
    char s[1024] = {0};
    size_t len = 0;
    if (end)
    {
        len = end - start;
    }
    else
    {
        len = strlen(start);
    }
    strncpy(s, start, len);

    const char * t = strchr(s, ':');
    if (t == NULL)
    {
        _host = string(s);
        if (strncasecmp(_protocol.c_str(), "http", 4) == 0)
        {
             _port = 80;
        }
        else if (strncasecmp(_protocol.c_str(), "https", 5) == 0)
        {
            _port = 443;
        }
        else if (strncasecmp(_protocol.c_str(), "rtmp", 4) == 0)
        {
            _port = 1935;
        }
        else
        {
            // TODO
        }
    }
    else
    {
        _host = string(s, t-s);
        _port = atoi(t+1);
    }
}

void URI::_parse_queries(const char *queries)
{
    const char *s = queries;

    while (*s == '&')
    {
        s++;
    }

    while (*s != '\0')
    {
        string k;
        string v;

        const char *p = strchr(s, '&');
        const char *t = strchr(s, '=');
        if (p == NULL)
        {
            if (t == NULL)
            {
                k = string(s);
            }
            else
            {
                if (t < s)
                {
                    break;
                }

                k = string(s, t-s);
                ++t;
                v = string(t);
            }
        }
        else
        {
            if (t == NULL)
            {
                if (p < s)
                {
                    break;
                }

                k =string(s, p-s);
            }
            else
            {
                if ((t < s)||(p < t))
                {
                    break;
                }

                k = string(s, t-s);
                ++t;
                v = string(t, p-t);
            }

        }

        string decoded_value = URI::uri_decode(v);

        _queries[k] = decoded_value;

        if (p == NULL)
        {
            break;
        }
        s = p + 1;
    }

}

string URI::get_query(string k)
{
    map<string, string>::const_iterator i = _queries.find(k);
    if (i != _queries.end())
    {
        return i->second;
    }
    return "";
}

string URI::uri_decode(const string &in)
{
    string out;
    for( size_t ix = 0; ix < in.size(); ix++ )
    {
        char ch = 0;
        if(in[ix]=='%')
        {
            ch = (fromHex(in[ix+1])<<4);
            ch |= fromHex(in[ix+2]);
            ix += 2;
        }
        else if(in[ix] == '+')
        {
            ch = ' ';
        }
        else
        {
            ch = in[ix];
        }
        out += (char)ch;
    }

    return out;
}
