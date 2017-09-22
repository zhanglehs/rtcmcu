#include "rtp_backend_config.h"
#include "../target.h"
#include "../../util/log.h"
#include "../../util/util.h"
#include "../util/access.h"
#include <assert.h>
#include "../target_config.h"

RTPBackendConfig::RTPBackendConfig() : 
    inited(false), has_rtp_conf(false), _rtp_conf()
{
    set_default_config();
}

RTPBackendConfig& RTPBackendConfig::operator=(const RTPBackendConfig& rhv)
{
    inited = rhv.inited;
    has_rtp_conf = rhv.has_rtp_conf;
    _rtp_conf = rhv._rtp_conf;
    memmove(listen_ip, rhv.listen_ip, sizeof(listen_ip));

    return *this;
}

void RTPBackendConfig::set_default_config()
{
    _rtp_conf.set_default_config();
    memset(listen_ip, 0, sizeof(listen_ip));
}

bool RTPBackendConfig::load_config(xmlnode* xml_config)
{
    if (inited)
        return true;

    ASSERTR(xml_config != NULL, false);
    xml_config = xmlgetchild(xml_config, module_name(), 0);
    ASSERTR(xml_config != NULL, false);

    has_rtp_conf = _rtp_conf.load_config(xml_config);

    char *q = NULL;

    q = xmlgetattr(xml_config, "listen_ip");
    if (q)
        strncpy(listen_ip, q, sizeof(listen_ip) - 1);

    inited = true;
    return resove_config();
}

bool RTPBackendConfig::reload() const
{
    bool ret = _rtp_conf.reload();

    return ret;
}

const char* RTPBackendConfig::module_name() const
{
    return "rtp_backend";
}

void RTPBackendConfig::dump_config() const
{
    _rtp_conf.dump_config();

    INF("rtp backend config: "
        "listen_ip=%s",
        listen_ip);
}

bool RTPBackendConfig::resove_config()
{
    char ip[32] = { '\0' };
    int ret;

    const TargetConfig* common_config = (const TargetConfig*)ConfigManager::get_inst_config_module("common");
    ASSERTR(common_config != NULL, false);

    if (strlen(listen_ip) == 0)
    {
        //if (strcmp("receiver", PROCESS_NAME) == 0)
        if (strstr(common_config->process_name, "receiver") != NULL)
        {
            if (g_private_cnt < 1)
            {
                fprintf(stderr, "backend listen ip not in conf and no private ip found\n");
                return false;
            }

            strncpy(listen_ip, g_private_ip[0], sizeof(listen_ip)-1);
        }
        //else if (strcmp("forward", PROCESS_NAME) == 0)
        else if (strstr(common_config->process_name, "forward") != NULL)
        {
            if (g_public_cnt < 1)
            {
                fprintf(stderr, "backend listen ip not in conf and no public ip found\n");
                return false;
            }

            strncpy(listen_ip, g_public_ip[0], sizeof(listen_ip)-1);
			//strncpy(listen_ip, "103.41.143.109", sizeof(listen_ip)-1);
        }
        else
        {
            fprintf(stderr, "not supported PROCESS_NAME %s\n", PROCESS_NAME);
            return false;
        }
    }

    strncpy(ip, listen_ip, sizeof(ip)-1);
    ret = util_interface2ip(ip, listen_ip, sizeof(listen_ip)-1);
    if (0 != ret)
    {
        fprintf(stderr, "backend listen ip resolv failed. host = %s, ret = %d\n", listen_ip, ret);
        return false;
    }

    return true;
}
