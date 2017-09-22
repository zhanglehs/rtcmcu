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

#include "base_http_server.h"
#include <string>
#include <assert.h>
#include <stdlib.h>
#include <json.h>
#include "perf.h"
#include "version.h"

using namespace std;
using namespace network;

#define HTTP_BUFFER_MAX 1024 * 1024
static char   g_configfile[PATH_MAX];
namespace http
{
    HTTPServerConfig::HTTPServerConfig()
    {
        set_default_config();
    }

    HTTPServerConfig::~HTTPServerConfig()
    {
    }

    HTTPServerConfig& HTTPServerConfig::operator=(const HTTPServerConfig& rhv)
    {
        memmove(this, &rhv, sizeof(HTTPServerConfig));
        return *this;
    }

    void HTTPServerConfig::set_default_config()
    {
        listen_port = 7086;
        timeout_second = 30;
        max_conns = 100;
        buffer_max = 1024 * 1024;
        memset(listen_port_cstr, 0, sizeof(listen_port_cstr));
    }

    bool HTTPServerConfig::load_config(xmlnode* xml_config)
    {
        ASSERTR(xml_config != NULL, false);
        xml_config = xmlgetchild(xml_config, module_name(), 0);
        ASSERTR(xml_config != NULL, false);

        if (inited)
            return true;

        const char *q = NULL;

        q = xmlgetattr(xml_config, "listen_port");
        if (!q)
        {
            fprintf(stderr, "http_server listen_port get failed.\n");
            return false;
        }
        listen_port = atoi(q);
        strcpy(listen_port_cstr, q);
        listen_port = (uint16_t)strtoul(q, NULL, 10);
        if (listen_port <= 0)
        {
            fprintf(stderr, "http_server listen_port not valid.\n");
            return false;
        }

        q = xmlgetattr(xml_config, "timeout_second");
        if (!q)
        {
            fprintf(stderr, "http_server timeout_second get failed.\n");
            return false;
        }
        timeout_second = (uint16_t)strtoul(q, NULL, 10);

        q = xmlgetattr(xml_config, "max_conns");
        if (!q)
        {
            fprintf(stderr, "http_server max_conns get failed.\n");
            return false;
        }
        max_conns = (uint16_t)strtoul(q, NULL, 10);

        q = xmlgetattr(xml_config, "buffer_max");
        if (!q)
        {
            fprintf(stderr, "http_server buffer_max get failed.\n");
            return false;
        }
        buffer_max = (uint16_t)strtoul(q, NULL, 10);

        inited = true;
        return true;
    }

    bool HTTPServerConfig::reload() const
    {
        return true;
    }

    const char* HTTPServerConfig::module_name() const
    {
        return "http_server";
    }

    void HTTPServerConfig::dump_config() const
    {
        //#todo
    }

    HTTPServer::HTTPServer(HTTPServerConfig* config,const char *config_file_path)
    {
        _config = config;
        _listen_port = config->listen_port;
		strcpy(g_configfile,config_file_path);
    }


    const char* HTTPServer::_get_server_name()
    {
        return "http";
    }

    void HTTPServer::_accept_error(const int fd, struct sockaddr_in& s_in)
    {
        util_http_rsp_error_code(fd, HTTP_500);
        ACCESS("http %s:%6hu 500", util_ip2str_no(s_in.sin_addr.s_addr),
            ntohs(s_in.sin_port));
    }

    TCPConnection* HTTPServer::_create_conn()
    {
        return new HTTPConnection(this, this->_main_base);
    }

    HTTPHandle* HTTPServer::_find_handle(HTTPConnection * conn)
    {
        HTTPRequest* req = conn->request;
        string& uri = req->uri;
		
        string str;
        int separative = 0;
        while (1)
        {
            if (uri[0] != '/')
            {
                break;
            }
            separative = uri.find('/', separative + 1);
            if (separative > 0)
            {
                str = uri.substr(0, separative);
                HttpMap_t::iterator it = _handler_map.find(str);
                if (it != _handler_map.end())
                {
                    return it->second;
                }
            }
            else
            {
				HttpMap_t::iterator it = _handler_map.find(uri);
				if (it != _handler_map.end())
				{
					return it->second;
				} else {
					break;
				}
            }
        }

        return NULL;
    }

    HTTPHandle* HTTPServer::add_handle(string start_path, http_cb call_back, void* arg)
    {
        if (start_path.length() == 0 || call_back == NULL)
        {
            // TODO: ERR
            return NULL;
        }

        HttpMap_t::iterator it = _handler_map.find(start_path);

        if (it != _handler_map.end())
        {
            return NULL;
        }

        HTTPHandle* handle = new HTTPHandle();
        handle->start_path = start_path;
        handle->callback.cb = call_back;
        handle->callback.cb_arg = arg;

        _handler_map[start_path] = handle;

        return handle;
    }

    HTTPHandle* HTTPServer::add_handle(string start_path, http_cb call_back, void* arg,
        http_cb first_line_cb, void* first_line_cb_arg,
        http_cb close_cb, void* close_cb_arg)
    {
        if ((start_path.length() == 0) || 
            ((call_back == NULL) && (first_line_cb == NULL) && (close_cb == NULL)))
        {
            // TODO: ERR
            return NULL;
        }

        HttpMap_t::iterator it = _handler_map.find(start_path);

        if (it != _handler_map.end())
        {
            ERR("this handle set already, path: %s", start_path.c_str());
            return NULL;
        }

        HTTPHandle* handle = new HTTPHandle();
        handle->start_path = start_path;
        handle->callback.first_line_cb = first_line_cb;
        handle->callback.first_line_cb_arg = first_line_cb_arg;
        handle->callback.cb = call_back;
        handle->callback.cb_arg = arg;
        handle->callback.close_cb = close_cb;
        handle->callback.close_cb_arg = close_cb_arg;

        _handler_map[start_path] = handle;

        return handle;
    }

    void HTTPServer::get_path_list_handler(HTTPConnection* conn, void* arg)
    {
        
    }
	void HTTPServer::get_path_list_check_handler(HTTPConnection* conn, void* args) {
		HTTPServer& server = *((HTTPServer*)args);
		HttpMap_t::iterator it;
		json_object* rsp = json_object_new_object();
		json_object* paths = json_object_new_array();

		for (it = server._handler_map.begin(); it != server._handler_map.end(); it++)
		{
			const string& handle = it->first;

			json_object* path = json_object_new_string(handle.c_str());
			json_object_array_add(paths, path);
		}

		json_object_object_add(rsp, "paths", paths);
		conn->writeResponse(rsp);
		json_object_put(rsp);
		conn->stop();
	}

	void HTTPServer::get_pid_handler(HTTPConnection* conn, void* args) {
		
	}

	void HTTPServer::get_pid_check_handler(HTTPConnection* conn, void* args) {
		pid_t pid = getpid();
		json_object* rsp = json_object_new_object();
		json_object* jpid = json_object_new_int(pid);
		json_object_object_add(rsp, "pid", jpid);
		conn->writeResponse(rsp);
		json_object_put(jpid);
		conn->stop();
	}

	void HTTPServer::get_version_handler(HTTPConnection* conn, void* args) {
		
	}

	void HTTPServer::get_version_check_handler(HTTPConnection* conn, void* args) {
		json_object* rsp = json_object_new_object();
		json_object* version = json_object_new_string(VERSION);
		json_object_object_add(rsp, "version", version);
		conn->writeResponse(rsp);
		json_object_put(version);
		conn->stop();
	}

	static void xml_to_json(struct xmlnode* xml, json_object* json)
	{
		int i;
		struct xmlattribute *xa;
		for (i = 0; i < xml->nattr; i++) {
			xa = xml->attr[i];
			json_object* value = json_object_new_string(xa->value);
			json_object_object_add(json, xa->name, value);
		}
		struct xmlnode *x;
		for (i = 0; i < xml->nchild; i++) {
			x = xml->child[i];
			json_object* j = json_object_new_object();
			xml_to_json(x, j);
			json_object_object_add(json, x->name, j);
		}
	}
	void HTTPServer::get_config_handler(HTTPConnection* conn, void* args) {
		
	}

	void HTTPServer::get_config_check_handler(HTTPConnection* conn, void* args) {
		json_object* rsp = json_object_new_object();
		struct xmlnode* mainnode = xmlloadfile(g_configfile);
		if (NULL == mainnode) {
			json_object* error = json_object_new_string("load config file error");
			json_object_object_add(rsp, "error", error);
		}
		xml_to_json(mainnode, rsp);
		freexml(mainnode);
		conn->writeResponse(rsp);
		json_object_put(rsp);
		conn->stop();
	}

	void HTTPServer::get_serverinfo_handler(HTTPConnection* conn, void* args) {
		
	}

	void HTTPServer::get_serverinfo_check_handler(HTTPConnection* conn, void* args) {
		json_object* rsp = json_object_new_object();
		Perf * perf = Perf::get_instance();
		sys_info_t sys_info = perf->get_sys_info();

		json_object* j_online_processors = json_object_new_int(sys_info.online_processors);
		json_object* j_mem = json_object_new_int(sys_info.mem_ocy);
		json_object* j_free_mem = json_object_new_int(sys_info.free_mem_ocy);
		json_object* j_cpu_rate = json_object_new_int(sys_info.cpu_rate);
		json_object_object_add(rsp, "online_processors", j_online_processors);
		json_object_object_add(rsp, "total_memory", j_mem);
		json_object_object_add(rsp, "free_memory", j_free_mem);
		json_object_object_add(rsp, "cpu_rate", j_cpu_rate);
		conn->writeResponse(rsp);
		json_object_put(rsp);
		conn->stop();
	}

    int HTTPServer::remove_handle(std::string start_path)
    {
        if (start_path.length() == 0 )
        {
            // TODO: ERR
            return -1;
        }

        HttpMap_t::iterator it = _handler_map.find(start_path);

        if (it != _handler_map.end())
        {
            return -1;
        }

        delete it->second;
        _handler_map.erase(it);

        return 0;
    }

	void HTTPServer::register_self_handle() {

		add_handle("/list", 
			get_path_list_handler, 
			this, 
			get_path_list_check_handler, 
			this, 
			NULL, 
			NULL);
		add_handle("/get_pid", 
			get_pid_handler, 
			this, 
			get_pid_check_handler, 
			this, 
			NULL, 
			NULL);
		add_handle("/get_version", 
			get_version_handler, 
			this, 
			get_version_check_handler, 
			this, 
			NULL, 
			NULL);
		add_handle("/get_config", 
			get_config_handler, 
			this, 
			get_config_check_handler, 
			this, 
			NULL, 
			NULL);
		add_handle("/get_server_info", 
			get_serverinfo_handler, 
			this, 
			get_serverinfo_check_handler, 
			this, 
			NULL, 
			NULL);
	}

}
