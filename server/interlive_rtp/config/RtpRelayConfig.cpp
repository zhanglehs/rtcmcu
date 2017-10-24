#include "config/RtpRelayConfig.h"

#include "target.h"
#include "config/TargetConfig.h"
#include "../util/log.h"
#include "../util/util.h"
#include <assert.h>

RtpRelayConfig::RtpRelayConfig() :
inited(false), has_rtp_conf(false), _rtp_conf()
{
  set_default_config();
}

RtpRelayConfig& RtpRelayConfig::operator=(const RtpRelayConfig& rhv)
{
  inited = rhv.inited;
  has_rtp_conf = rhv.has_rtp_conf;
  _rtp_conf = rhv._rtp_conf;

  return *this;
}

void RtpRelayConfig::set_default_config()
{
  _rtp_conf.set_default_config();
}

bool RtpRelayConfig::load_config(xmlnode* xml_config)
{
  if (inited)
    return true;

  ASSERTR(xml_config != NULL, false);
  xml_config = xmlgetchild(xml_config, module_name(), 0);
  ASSERTR(xml_config != NULL, false);

  has_rtp_conf = _rtp_conf.load_config(xml_config);

  inited = true;
  return resove_config();
}

bool RtpRelayConfig::reload() const
{
  bool ret = _rtp_conf.reload();
  return ret;
}

const char* RtpRelayConfig::module_name() const
{
  return "rtp_relay";
}

void RtpRelayConfig::dump_config() const
{
  _rtp_conf.dump_config();
}

bool RtpRelayConfig::resove_config()
{
  return true;
}
