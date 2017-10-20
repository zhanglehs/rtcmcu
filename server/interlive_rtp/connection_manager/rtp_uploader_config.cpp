#include "rtp_uploader_config.h"
#include "target.h"
#include "util/log.h"
#include "util/util.h"
#include <assert.h>

RTPUploaderConfig::RTPUploaderConfig() :
    inited(false), 
    _rtp_conf(), 
    listen_port(0)
{
    memset(listen_host, 0, sizeof(listen_host));
    memset(listen_ip, 0, sizeof(listen_ip));
    memset(listen_port_cstr, 0, sizeof(listen_port_cstr));
    memset(private_key, 0, sizeof(private_key));
    memset(super_key, 0, sizeof(super_key));
}

RTPUploaderConfig& RTPUploaderConfig::operator=(const RTPUploaderConfig& rhv)
{
    inited = rhv.inited;
    listen_port = rhv.listen_port;
    memcpy(listen_host, rhv.listen_host, sizeof(listen_host));
    memcpy(listen_ip, rhv.listen_ip, sizeof(listen_ip));
    memcpy(listen_port_cstr, rhv.listen_port_cstr, sizeof(listen_port_cstr));
    memcpy(private_key, rhv.private_key, sizeof(private_key));
    memcpy(super_key, rhv.super_key, sizeof(super_key));
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

    q = xmlgetattr(xml_config, "listen_port");
    if (!q)
    {
        fprintf(stderr, "rtp uploader listen_port get failed.\n");
        return false;
    }
    listen_port = atoi(q);
    strcpy(listen_port_cstr, q);
    if (listen_port <= 0)
    {
        fprintf(stderr, "rtp uploader listen_port not valid.\n");
        return false;
    }

    q = xmlgetattr(xml_config, "private_key");
    if (!q)
    {
        fprintf(stderr, "rtp uploader private_key get failed.\n");
        return false;
    }
    strncpy(private_key, q, sizeof(private_key));

    q = xmlgetattr(xml_config, "super_key");
    if (!q)
    {
        fprintf(stderr, "rtp uploader super_key get failed.\n");
        return false;
    }
    strncpy(super_key, q, sizeof(super_key));

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
