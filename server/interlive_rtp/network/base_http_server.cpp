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

#include "config/HttpServerConfig.h"
#include <string>
#include <assert.h>
#include <stdlib.h>
#include <json.h>
#include "perf.h"
#include "version.h"
#include "../util/log.h"

HttpServerManager* HttpServerManager::m_inst = NULL;

HttpServerManager* HttpServerManager::Instance() {
  if (m_inst) {
    return m_inst;
  }
  m_inst = new HttpServerManager();
  return m_inst;
}

void HttpServerManager::DestroyInstance() {
  if (m_inst) {
    delete m_inst;
    m_inst = NULL;
  }
}

HttpServerManager::HttpServerManager() {
  m_ev_base = NULL;
  m_ev_http = NULL;
}

HttpServerManager::~HttpServerManager() {
  if (m_ev_http) {
    evhttp_free(m_ev_http);
  }
}

int HttpServerManager::Init(struct event_base *ev_base) {
  HTTPServerConfig* config = (HTTPServerConfig*)ConfigManager::get_inst_config_module("http_server");
  if (config == NULL) {
    return -1;
  }

  m_ev_base = ev_base;
  m_ev_http = evhttp_new(m_ev_base);
  int ret = evhttp_bind_socket(m_ev_http, "0.0.0.0", config->listen_port);
  if (ret != 0) {
    return -1;
  }
  evhttp_set_timeout(m_ev_http, 10);
  evhttp_set_gencb(m_ev_http, DefaultHandler, NULL);
  return 0;
}

int HttpServerManager::AddHandler(const char *path, void(*cb)(struct evhttp_request *, void *), void *cb_arg) {
  return evhttp_set_cb(m_ev_http, path, cb, cb_arg);
}

void HttpServerManager::DefaultHandler(struct evhttp_request *requset, void *arg) {
  const struct evhttp_uri *uri = evhttp_request_get_evhttp_uri(requset);
  if (uri == NULL) {
    return;
  }
  //const char *query = evhttp_uri_get_query(uri);
  const char *path = evhttp_uri_get_path(uri);
  if (path == NULL) {
    return;
  }

  if (strcmp(path, "/get_version") == 0) {
    json_object* root = json_object_new_object();
    json_object_object_add(root, "version", json_object_new_string(VERSION));

    const char *content = json_object_to_json_string(root);
    struct evbuffer *buf = evbuffer_new();
    evbuffer_add(buf, content, strlen(content));
    evhttp_send_reply(requset, HTTP_OK, "OK", buf);
    evbuffer_free(buf);
    json_object_put(root);
  }
  else if (strcmp(path, "/get_config") == 0) {
  }
  else if (strcmp(path, "/get_server_info") == 0) {
    Perf * perf = Perf::get_instance();
    sys_info_t sys_info = perf->get_sys_info();
    json_object* root = json_object_new_object();
    json_object* j_online_processors = json_object_new_int(sys_info.online_processors);
    json_object* j_mem = json_object_new_int(sys_info.mem_ocy);
    json_object* j_free_mem = json_object_new_int(sys_info.free_mem_ocy);
    json_object* j_cpu_rate = json_object_new_int(sys_info.cpu_rate);
    json_object_object_add(root, "online_processors", j_online_processors);
    json_object_object_add(root, "total_memory", j_mem);
    json_object_object_add(root, "free_memory", j_free_mem);
    json_object_object_add(root, "cpu_rate", j_cpu_rate);

    const char *content = json_object_to_json_string(root);
    struct evbuffer *buf = evbuffer_new();
    evbuffer_add(buf, content, strlen(content));
    evhttp_send_reply(requset, HTTP_OK, "OK", buf);
    evbuffer_free(buf);
    json_object_put(root);
  }
}
