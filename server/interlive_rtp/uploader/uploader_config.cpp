#include "uploader_config.h"
#include "target.h"
#include "util/log.h"
#include "util/util.h"
#include <assert.h>

uploader_config::uploader_config()
{
    inited = false;
    set_default_config();
}

uploader_config::~uploader_config()
{
}

uploader_config& uploader_config::operator=(const uploader_config& rhv)
{
    memmove(this, &rhv, sizeof(uploader_config));
    return *this;
}

void uploader_config::set_default_config()
{
    memset(listen_ip, 0, sizeof(listen_ip));
    listen_port = 10000;
    timeout = 30;
    buffer_max = 2 * 1024 * 1024;
    check_point_block_cnt_min = 27;
    check_point_dura = 3;
    rpt_dura = 5 * 60;
    memset(private_key, 0, sizeof(private_key));
    memset(super_key, 0, sizeof(super_key));
}

bool uploader_config::load_config(xmlnode* xml_config)
{
    ASSERTR(xml_config != NULL, false);
    xmlnode *p = xmlgetchild(xml_config, "uploader", 0);
    ASSERTR(p != NULL, false);

    return load_config_unreloadable(p) && load_config_reloadable(p) && resove_config();
}

bool uploader_config::reload() const
{
    return true;
}

const char* uploader_config::module_name() const
{
    return "uploader";
}

void uploader_config::dump_config() const
{
    INF("uploader config: "
        "listen_ip=%s, listen_port=%u, timeout=%d, private_key=%s, super_key=%s, "
        "buffer_max=%lu, check_point_block_cnt_min=%d, check_point_dura=%d, rpt_dura=%d",
        listen_ip, uint32_t(listen_port), timeout, private_key, super_key,
        buffer_max, check_point_block_cnt_min, check_point_dura, rpt_dura);
}

bool uploader_config::load_config_unreloadable(xmlnode* xml_config)
{
    if (inited)
        return true;

    const char *q = NULL;
    int tmp = 0;

    q = xmlgetattr(xml_config, "listen_host");
    if (q)
    {
        strncpy(listen_ip, q, 31);
    }

    q = xmlgetattr(xml_config, "listen_port");
    if (!q)
    {
        fprintf(stderr, "uploader_listen_port get failed.\n");
        return false;
    }
    listen_port = atoi(q);
    if (listen_port <= 0)
    {
        fprintf(stderr, "uploader_listen_port not valid.\n");
        return false;
    }

    q = xmlgetattr(xml_config, "buffer_max");
    if (!q)
    {
        fprintf(stderr, "uploader buffer_max get failed.\n");
        return false;
    }
    tmp = atoi(q);
    if (tmp <= 0 || tmp >= 20 * 1024 * 1024)
    {
        fprintf(stderr, "uploader buffer_max not valid. value = %d\n", tmp);
        return false;
    }
    buffer_max = tmp;

    inited = true;
    return true;
}

bool uploader_config::load_config_reloadable(xmlnode* xml_config)
{
    const char *q = NULL;

    q = xmlgetattr(xml_config, "super_key");
    if (!q)
    {
        fprintf(stderr, "player super_key get failed.\n");
        return false;
    }
    strncpy(super_key, q, sizeof(super_key)-1);

    q = xmlgetattr(xml_config, "timeout");
    if (!q)
    {
        fprintf(stderr, "uploader_timeout get failed.\n");
        return false;
    }
    timeout = atoi(q);
    if (timeout <= 0)
    {
        fprintf(stderr, "uploader_timeout not valid.\n");
        return false;
    }

    q = xmlgetattr(xml_config, "check_point_block_cnt_min");
    if (!q)
    {
        fprintf(stderr, "uploader check_point_block_cnt_min get failed.\n");
        return false;
    }
    check_point_block_cnt_min = atoi(q);
    if (check_point_block_cnt_min <= 0)
    {
        fprintf(stderr, "uploader check_point_block_cnt_min not valid.\n");
        return false;
    }

    q = xmlgetattr(xml_config, "check_point_dura");
    if (!q)
    {
        fprintf(stderr, "uploader check_point_dura get failed.\n");
        return false;
    }
    check_point_dura = atoi(q);
    if (check_point_dura <= 0)
    {
        fprintf(stderr, "uploader check_point_dura not valid.\n");
        return false;
    }

    q = xmlgetattr(xml_config, "rpt_dura");
    if (!q)
    {
        fprintf(stderr, "uploader rpt_dura get failed.\n");
        return false;
    }
    rpt_dura = atoi(q);
    if (rpt_dura <= 0) {
        fprintf(stderr, "uploader rpt_dura not valid.\n");
        return false;
    }

    q = xmlgetattr(xml_config, "private_key");
    if (!q)
    {
        fprintf(stderr, "uploader private_key get failed.\n");
        return false;
    }
    strncpy(private_key, q, sizeof(private_key)-1);

    return true;
}

bool uploader_config::resove_config()
{
    char ip[32] = { '\0' };
    int ret;

    if (strlen(listen_ip) == 0)
    {
        if (g_public_cnt < 1)
        {
            fprintf(stderr, "uploader listen ip not in conf and no public ip found\n");
            return false;
        }
        strncpy(listen_ip, g_public_ip[0], sizeof(listen_ip)-1);
    }

    strncpy(ip, listen_ip, sizeof(ip)-1);
    ret = util_interface2ip(ip, listen_ip, sizeof(listen_ip)-1);
    if (0 != ret)
    {
        fprintf(stderr, "uploader_listen_host resolv failed. host = %s, ret = %d\n", listen_ip, ret);
        return false;
    }

    return true;
}

