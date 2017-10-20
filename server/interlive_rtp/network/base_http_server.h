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

#include "config_manager.h"
#include <event.h>
#include <event2/http.h>

class HTTPServerConfig : public ConfigModule {
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

class HttpServerManager {
public:
  static HttpServerManager* Instance();
  static void DestroyInstance();

  int Init(struct event_base *ev_base);
  int AddHandler(const char *path,
    void(*cb)(struct evhttp_request *, void *), void *cb_arg);

protected:
  HttpServerManager();
  ~HttpServerManager();
  static void DefaultHandler(struct evhttp_request *requset, void *arg);

  struct event_base* m_ev_base;
  struct evhttp *m_ev_http;
  static HttpServerManager* m_inst;
};
