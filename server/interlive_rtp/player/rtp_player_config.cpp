#include "rtp_player_config.h"
#include "util/log.h"
#include "util/util.h"
#include <assert.h>

RTPPlayerConfig::RTPPlayerConfig() :
    inited(false), 
    _rtp_conf(),
    listen_port(0)
{
    memset(listen_host, 0, sizeof(listen_host));
    memset(listen_ip, 0, sizeof(listen_ip));
    memset(private_key, 0, sizeof(private_key));
    memset(super_key, 0, sizeof(super_key));
}

RTPPlayerConfig& RTPPlayerConfig::operator=(const RTPPlayerConfig& rhv)
{
    inited = rhv.inited;
    listen_port = rhv.listen_port;
    memcpy(listen_host, rhv.listen_host, sizeof(listen_host));
    memcpy(listen_ip, rhv.listen_ip, sizeof(listen_ip));
    memcpy(private_key, rhv.private_key, sizeof(private_key));
    memcpy(super_key, rhv.super_key, sizeof(super_key));
    _rtp_conf = rhv._rtp_conf;
    return *this;
}

void RTPPlayerConfig::set_default_config()
{
    _rtp_conf.set_default_config();
}

bool RTPPlayerConfig::load_config(xmlnode* xml_config)
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
        fprintf(stderr, "rtp player listen_ip get failed.\n");
        return false;
    }

    q = xmlgetattr(xml_config, "listen_port");
    if (!q)
    {
        fprintf(stderr, "player listen_port get failed.\n");
        return false;
    }
    listen_port = atoi(q);
    if (listen_port <= 0)
    {
        fprintf(stderr, "player listen_port not valid.\n");
        return false;
    }

    q = xmlgetattr(xml_config, "private_key");
    if (!q)
    {
        fprintf(stderr, "player private_key get failed.\n");
        return false;
    }
    strncpy(private_key, q, sizeof(private_key));

    q = xmlgetattr(xml_config, "super_key");
    if (!q)
    {
        fprintf(stderr, "player super_key get failed.\n");
        return false;
    }
    strncpy(super_key, q, sizeof(super_key));

    inited = true;
    return ret;
}

bool RTPPlayerConfig::reload() const
{
    bool ret = _rtp_conf.reload();

    return ret;
}

const char* RTPPlayerConfig::module_name() const
{
    return "rtp_player";
}

void RTPPlayerConfig::dump_config() const
{
    // #TODO
    _rtp_conf.dump_config();
}

RTPTransConfig &RTPPlayerConfig::get_rtp_trans_config()
{
    return _rtp_conf;
}
