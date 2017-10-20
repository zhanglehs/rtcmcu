#include "rtp_player_config.h"
#include "../util/log.h"
#include "../util/util.h"
#include <assert.h>

RTPPlayerConfig::RTPPlayerConfig() :
    inited(false), 
    _rtp_conf()
{
}

RTPPlayerConfig& RTPPlayerConfig::operator=(const RTPPlayerConfig& rhv)
{
    inited = rhv.inited;
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
