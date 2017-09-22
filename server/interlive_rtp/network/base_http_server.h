/**
* @file
* @brief
* @author   songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2015/07/24
* @see
*/


#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <errno.h>

#include <time.h>
#include <stdint.h>
#include <string>
#include <map>

#include "util/util.h"
#include "utils/buffer.hpp"
#include "util/levent.h"
#include "util/log.h"
#include "util/access.h"

#include "streamid.h"
#include "http_request.h"
#include "http_connection.h"
#include "base_tcp_server.h"
#include "config_manager.h"
#include <set>

#define MAX_HTTP_REQ_LINE (16 * 1024)

namespace http
{
    class HTTPServerConfig : public ConfigModule
    {
    private:
        bool inited;

    public:
        uint16_t listen_port;
        char listen_port_cstr[8];
        int timeout_second;
        int max_conns;
        size_t buffer_max;

    public:
        HTTPServerConfig();
        virtual ~HTTPServerConfig();
        HTTPServerConfig& operator=(const HTTPServerConfig& rhv);
        virtual void set_default_config();
        virtual bool load_config(xmlnode* xml_config);
        virtual bool reload() const;
        virtual const char* module_name() const;
        virtual void dump_config() const;
    };

   // static void get_path_list_handler

    class HTTPServer : public network::TCPServer
    {
        friend class HTTPConnection;
    public:
        HTTPServer(HTTPServerConfig* config,const char *config_file_path);
        HTTPHandle* add_handle(std::string start_path, http_cb call_back, void* arg);
        HTTPHandle* add_handle(std::string start_path, http_cb call_back, void* arg,
            http_cb first_line_cb, void* first_line_cb_arg, 
            http_cb close_cb, void* close_cb_arg);
        int remove_handle(std::string start_path);
		void register_self_handle();

        static void get_path_list_handler(HTTPConnection* conn, void* args);
		static void get_path_list_check_handler(HTTPConnection* conn, void* args);
		static void get_pid_handler(HTTPConnection* conn, void* args);
		static void get_pid_check_handler(HTTPConnection* conn, void* args);
		static void get_version_handler(HTTPConnection* conn, void* args);
		static void get_version_check_handler(HTTPConnection* conn, void* args);
		static void get_config_handler(HTTPConnection* conn, void* args);
		static void get_config_check_handler(HTTPConnection* conn, void* args);
		static void get_serverinfo_handler(HTTPConnection* conn, void* args);
		static void get_serverinfo_check_handler(HTTPConnection* conn, void* args);

    protected:
        HTTPHandle* _find_handle(HTTPConnection * c);

        virtual const char* _get_server_name();
        virtual void _accept_error(const int fd, struct sockaddr_in& s_in);
        virtual network::TCPConnection* _create_conn();

    protected:
        HTTPServerConfig* _config;

        typedef  std::map<std::string, HTTPHandle*> HttpMap_t;
        HttpMap_t _handler_map;
    };

};
