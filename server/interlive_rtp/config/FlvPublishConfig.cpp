#include "config/FlvPublishConfig.h"

#include "../util/log.h"
#include <string.h>

ModPublisherConfig::ModPublisherConfig()
{
  inited = false;
  memset(flv_receiver_ip, 0, sizeof(flv_receiver_ip));
  flv_receiver_port = 0;
}

ModPublisherConfig& ModPublisherConfig::operator=(const ModPublisherConfig& rhv)
{
  memmove(this, &rhv, sizeof(ModPublisherConfig));
  return *this;
}

void ModPublisherConfig::set_default_config()
{
  strcpy(flv_receiver_ip, "127.0.0.1");
  flv_receiver_port = 80;
}

bool ModPublisherConfig::load_config(xmlnode* xml_config)
{
  xmlnode *p = xmlgetchild(xml_config, module_name(), 0);

  if (inited || !p)
    return true;

  char *q = NULL;

  q = xmlgetattr(p, "flv_receiver_ip");
  if (q)
  {
    strncpy(flv_receiver_ip, q, 31);
  }


  q = xmlgetattr(p, "flv_receiver_port");
  if (q)
  {
    flv_receiver_port = atoi(q);
    if (flv_receiver_port <= 0)
    {
      ERR("publisher tacker_port not valid.");
      return false;
    }
  }

  inited = true;
  return true;
}

bool ModPublisherConfig::reload() const
{
  return true;
}

const char* ModPublisherConfig::module_name() const
{
  return "flv_publisher";
}

void ModPublisherConfig::dump_config() const
{
  INF("tracker config: " "flv_receiver_ip=%s, flv_receiver_port=%hu", flv_receiver_ip, flv_receiver_port);
}
