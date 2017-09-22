#ifndef _UPLOADER_CONFIG_H_
#define _UPLOADER_CONFIG_H_

#include "config_manager.h"
#include "util/xml.h"
#include <stdint.h>

class uploader_config : public ConfigModule
{
private:
    bool inited;

public:
    char listen_ip[32];
    uint16_t listen_port;
    int timeout;
    char private_key[64];
    char super_key[36];
    size_t buffer_max;
    int check_point_block_cnt_min;
    int check_point_dura;
    int rpt_dura;

public:
    uploader_config();
    virtual ~uploader_config();
    uploader_config& operator=(const uploader_config& rhv);
    virtual void set_default_config();
    virtual bool load_config(xmlnode* xml_config);
    virtual bool reload() const;
    virtual const char* module_name() const;
    virtual void dump_config() const;

private:
    bool load_config_unreloadable(xmlnode* xml_config);
    bool load_config_reloadable(xmlnode* xml_config);
    bool resove_config();
};

#endif
