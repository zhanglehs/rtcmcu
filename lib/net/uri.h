/*
 * a simple URI parser
 * author: hechao@youku.com
 * create: 2014/3/18
 *
 */

#ifndef __URI_H_
#define __URI_H_    
#include <string>
#include <map>

class URI
{
    public:
        static std::string uri_decode(const std::string &in);
    public:
        URI();

        int parse(const std::string& uri);
        int parse(const char *uri);
        int parse_path(const char * path);
        int parse_path(const std::string &path);
        std::string get_protocol() const {return _protocol;}
        std::string get_host() const {return _host;}
        std::string get_path() const {return _path;}
        std::string get_file() const {return _file;}
        short get_port() const {return _port;}
        bool has_param(std::string k) { return _queries.find(k) != _queries.end();}
        std::string get_query(std::string k);

    private:
        void _parse_queries(const char *q);
        void _parse_host_port(const char *s, const char *e);

    private:
        std::string _protocol, _host, _path, _file;
        short _port;
        std::map<std::string, std::string> _queries;
};

#endif /* __URI_H_ */
