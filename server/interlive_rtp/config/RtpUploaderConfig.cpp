#include "config/RtpUploaderConfig.h"

#include "target.h"
#include "../util/log.h"
#include "../util/util.h"
#include <assert.h>

RTPUploaderConfig::RTPUploaderConfig() :
    inited(false), 
    _rtp_conf(), 
    listen_udp_port(0)
{
  listen_tcp_port = 0;
    memset(listen_host, 0, sizeof(listen_host));
    memset(listen_ip, 0, sizeof(listen_ip));
}

RTPUploaderConfig& RTPUploaderConfig::operator=(const RTPUploaderConfig& rhv)
{
    inited = rhv.inited;
    listen_udp_port = rhv.listen_udp_port;
    memcpy(listen_host, rhv.listen_host, sizeof(listen_host));
    memcpy(listen_ip, rhv.listen_ip, sizeof(listen_ip));
    _rtp_conf = rhv._rtp_conf;
    return *this;
}

void RTPUploaderConfig::set_default_config()
{
    _rtp_conf.set_default_config();
}

bool RTPUploaderConfig::load_config(xmlnode* xml_config)
{
    ASSERTR(xml_config != NULL, false);
    xml_config = xmlgetchild(xml_config, module_name(), 0);
    ASSERTR(xml_config != NULL, false);

    bool ret = _rtp_conf.load_config(xml_config);

    char *q = NULL;
    q = xmlgetattr(xml_config, "listen_host");
    if (q)
    {
        strncpy(listen_host, q, 31);
    }
    int res = util_interface2ip(listen_host, listen_ip, sizeof(listen_ip)-1);
    if (0 != res)
    {
        fprintf(stderr, "rtp uploader listen_ip get failed.\n");
        return false;
    }

    q = xmlgetattr(xml_config, "listen_udp_port");
    if (!q)
    {
        fprintf(stderr, "rtp uploader listen_udp_port get failed.\n");
        return false;
    }
    listen_udp_port = atoi(q);
    if (listen_udp_port <= 0)
    {
        fprintf(stderr, "rtp uploader listen_udp_port not valid.\n");
        return false;
    }

    q = xmlgetattr(xml_config, "listen_tcp_port");
    if (!q)
    {
      fprintf(stderr, "rtp uploader listen_tcp_port get failed.\n");
      return false;
    }
    listen_tcp_port = atoi(q);
    if (listen_tcp_port <= 0)
    {
      fprintf(stderr, "rtp uploader listen_tcp_port not valid.\n");
      return false;
    }

    inited = true;
    return ret;
}

bool RTPUploaderConfig::reload() const
{
    bool ret = _rtp_conf.reload();
    return ret;
}

const char* RTPUploaderConfig::module_name() const
{
    return "rtp_uploader";
}

void RTPUploaderConfig::dump_config() const
{
    // #TODO
    _rtp_conf.dump_config();
}

RTPTransConfig &RTPUploaderConfig::get_rtp_trans_config()
{
    return _rtp_conf;
}
