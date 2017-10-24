#include "config/HttpServerConfig.h"

#include "../util/log.h"
#include <string.h>

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
