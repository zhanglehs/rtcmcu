#ifndef _RTP_UPLOADER_CONFIG_H_
#define _RTP_UPLOADER_CONFIG_H_

#include "config_manager.h"
#include "../util/xml.h"
#include "rtp_trans/rtp_config.h"

class RTPUploaderConfig : public ConfigModule
{
private:
    bool inited;
    RTPTransConfig _rtp_conf;
public:
    char    listen_host[32];
    char    listen_ip[32];
    int     listen_udp_port;
    int     listen_tcp_port;

public:
    RTPUploaderConfig();
    virtual ~RTPUploaderConfig() {}
    RTPUploaderConfig& operator=(const RTPUploaderConfig& rhv);

    virtual void set_default_config();
    virtual bool load_config(xmlnode* xml_config);
    virtual bool reload() const;
    virtual const char* module_name() const;
    virtual void dump_config() const;

    RTPTransConfig &get_rtp_trans_config();
};

#endif
