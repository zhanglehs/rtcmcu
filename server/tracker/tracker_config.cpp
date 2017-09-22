/*
 * author: hechao@youku.com
 * create: 2013.7.23
 */

#include "tracker_config.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <util/log.h>
#include <utils/memory.h>
#include <util/util.h>
#include <util/xml.h>

#include "tracker_def.h"
#include "version.h"

#include <set>

tracker_config_t  *g_config;
forwards_config_t *g_forwards_config;
group_config_t    *g_group_config;

int
tracker_config_init()
{
    if (!g_config)
    {
        g_config = (tracker_config_t*)mcalloc(1, sizeof(tracker_config_t));
        if (!g_config)
        {
            ERR("calloc tracker config struct error");
            return -1;
        }
        memset((void*)g_config, 0, sizeof(tracker_config_t));
    }

    if (!g_forwards_config)
    {
        g_forwards_config = (forwards_config_t *)mcalloc(1, sizeof(forwards_config_t));
        if (!g_forwards_config)
        {
            ERR("calloc forwards_config_t struct error");
            goto err;
        }
        INIT_LIST_HEAD(&g_forwards_config->root);
    }

    if (!g_group_config)
    {
        g_group_config = (group_config_t *)mcalloc(1, sizeof(group_config_t));
        if (!g_group_config)
        {
            ERR("calloc group_config_t struct error");
            goto err;
        }
        INIT_LIST_HEAD(&g_group_config->root);
    }

    return 0;
err:
    if (g_forwards_config)
    {
        mfree(g_forwards_config);
        g_forwards_config = 0;
    }

    if (g_group_config)
    {
        mfree(g_group_config);
        g_group_config = 0;
    }

    return -1;
}

int tracker_parse_config()
{
    tracker_config_t * config = g_config;
    if (!config || config->configfile[0] == '\0')
    {
        return -1;
    }

    char* q = NULL;

    struct xmlnode* mainnode = xmlloadfile(config->configfile);
    if (NULL == mainnode)
    {
        ERR("mainnode is NULL.");
        return -1;
    }

    /* tracker */
    struct xmlnode* root = xmlgetchild(mainnode, "tracker", 0);
    if (!root)
    {
        ERR("node tracker get failed. ");
        return -1;
    }
    /* tracker/common */
    struct xmlnode *common = xmlgetchild(root, "common", 0);
    if (!common)
    {
        ERR("common get failed.");
        return -1;
    }
    /* tracker/common/server */
    struct xmlnode *server = xmlgetchild(common, "server", 0);
    /* tracker/common/server:listen_ip */
    q = xmlgetattr(server, "listen_ip");
    if (!q)
    {
        ERR("listen_ip get failed.");
        return -1;
    }
    snprintf(config->listen_ip, strlen(q)+1, "%s", q);
    INF("listen_ip: %s", config->listen_ip);

    char ip[32];
    memset(ip, 0, sizeof(ip));
    strncpy(ip, config->listen_ip, 31);
    int ret = util_interface2ip(ip, config->listen_ip, 31);
    if(0 != ret) {
        fprintf(stderr,
                "listen_ip resolv failed. host = %s, ret = %d\n",
                config->listen_ip, ret);
        return -1;
    }

    /* tracker/common/server:listen_port */
    q = xmlgetattr(server, "listen_port");
    if (!q)
    {
        ERR("listen_port get failed.");
        return -1;
    }
    config->listen_port = atoi(q);

    /* tracker/common/server:portal_ip */
    q = xmlgetattr(server, "portal_ip");
    if (!q)
    {
        ERR("portal_ip get failed.");
        return -1;
    }
    snprintf(config->portal_ip, strlen(q) + 1, "%s", q);
    INF("portal_ip: %s", config->portal_ip);

    memset(ip, 0, sizeof(ip));
    strncpy(ip, config->portal_ip, 31);
    ret = util_interface2ip(ip, config->portal_ip, 31);
    if (0 != ret) {
        fprintf(stderr,
            "portal_ip resolv failed. host = %s, ret = %d\n",
            config->portal_ip, ret);
        return -1;
    }

    /* tracker/common/server:portal_port */
    q = xmlgetattr(server, "portal_port");
    if (!q)
    {
        ERR("portal_port get failed.");
        return -1;
    }
    config->portal_port = atoi(q);

    /* tracker/common/server:notifier_listen_port */
    q = xmlgetattr(server, "notifier_listen_port");
    if (!q)
    {
        ERR("notifier_listen_port get failed.");
        return -1;
    }
    config->notifier_listen_port = atoi(q);

    /* tracker/common/server:http_listen_port */
    q = xmlgetattr(server, "http_listen_port");
    if (!q)
    {
        ERR("http_listen_port get failed.");
        return -1;
    }
    config->http_listen_port = atoi(q);


    q = xmlgetattr(server, "child_topo_level");
    if (!q)
    {
        ERR("child topo level get failed.");
        return -1;
    }
    config->child_topo_level = atoi(q);

    /* tracker/common/log */
    struct xmlnode *log_node = xmlgetchild(common, "log", 0);
    if (!log_node)
    {
        ERR("log node get failed.");
        return -1;
    }
    q = xmlgetattr(log_node, "dir");
    if (!q)
    {
        ERR("log dir attr get failed.");
        return -1;
    }
    snprintf(config->log_dir, strlen(q)+1, "%s", q);
    q = xmlgetattr(log_node, "name");
    if (!q)
    {
        ERR("log name attr get failed.");
        return -1;
    }
    snprintf(config->log_name, strlen(q)+1, "%s", q);
    q = xmlgetattr(log_node, "level");
    if (!q)
    {
        ERR("log level attr get failed.");
        return -1;
    }
    config->log_level = log_str2level(q);
    q = xmlgetattr(log_node, "server_ip");
    if (!q)
    {
        ERR("log server ip attr get failed.");
        return -1;
    }
    snprintf(config->log_svr_ip, strlen(q)+1, "%s", q);
    q = xmlgetattr(log_node, "server_port");
    if (!q)
    {
        ERR("log server port attr get failed.");
        return -1;
    }
    config->log_svr_port = atoi(q);
    /* tracker/common */
    struct xmlnode *forwards = xmlgetchild(root, "forwards",0);
    if (!forwards)
    {
        ERR("forwards get failed.");
        return -1;
    }
    int cnt = 0;
    for (; ; cnt++)
    {
        struct xmlnode * forward_set = xmlgetchild(forwards, "forward_set", cnt);
        if (!forward_set)
            break;
        forward_set_t * fs = (forward_set_t *)mcalloc(1, sizeof(forward_set_t));
        if (!fs)
        {
            ERR("forward set struct calloc failed.");
            return -1;
        }
        INIT_LIST_HEAD(&fs->next);
        list_add_tail(&fs->next, &g_forwards_config->root);

        q = xmlgetattr(forward_set, "start_ip");
        if (!q || 0 != util_str2ip(q, &fs->start_ip))
        {
            ERR("start_ip get failed.");
            return -1;
        }
        q = xmlgetattr(forward_set, "stop_ip");
        if (!q || 0 != util_str2ip(q, &fs->stop_ip))
        {
            ERR("stop_ip get failed.");
            return -1;
        }
        q = xmlgetattr(forward_set, "asn");
        if (!q)
        {
            ERR("asn get failed.");
            return -1;
        }
        fs->asn = atoi(q);
        q = xmlgetattr(forward_set, "region");
        if (!q)
        {
            ERR("region get failed.");
            return -1;
        }
        fs->region = atoi(q);
        q = xmlgetattr(forward_set, "topo_level");
        if (!q)
        {
            WRN("topo_level get failed.");
            fs->topo_level = -1;
            //return -1;
        }
        else
            fs->topo_level = atoi(q);
    }

    /* tracker/group */
    struct xmlnode * group = xmlgetchild(root, "group", 0);
    if (group)
    {
        for (cnt=0; ; cnt++)
        {
            /* tracker/group/g_lead */
            struct xmlnode * g_lead = xmlgetchild(group, "g_lead", cnt);
            if (!g_lead)
                break;
            group_t * ng = (group_t*)mcalloc(1, sizeof(group_t));
            if (!ng)
            {
                ERR("g_lead get failed.");
                return -1;
            }
            INIT_LIST_HEAD(&ng->next);
            ng->lead_ip = 0;
            memset(&ng->alias, 0, sizeof(ng->alias));
            list_add_tail(&ng->next, &g_group_config->root);

            q = xmlgetattr(g_lead, "ip");
            if (!q || 0 != util_str2ip(q, &ng->lead_ip))
            {
                ERR("g_lead/ip get failed.");
                return -1;
            }

            q = xmlgetattr(g_lead, "alias");
            if (!q)
            {
                ERR("g_lead/alias get failed.");
                return -1;
            }
            snprintf(ng->alias, strlen(q)+1, "%s", q);
        }
    }
    return 0;

//ret_line:
//    return -1;
}

int
tc_get_asn_and_region_and_level(uint32_t ip, uint16_t *asn,
        uint16_t *region, uint8_t *level)
{
    list_head_t *pos;
    list_for_each(pos, &(g_forwards_config->root))
    {
        forward_set_t * fs = list_entry(pos, forward_set_t, next);
        if (ip >= fs->start_ip
                && ip <= fs->stop_ip)
        {
            *asn = fs->asn;
            *region = fs->region;
            *level = fs->topo_level;
            return 0;
        }
    }

    return -1;
}

int
tc_get_total_region_number()
{
    std::set<uint16_t> region_set;
    std::set<uint16_t>::iterator it;

    list_head_t *pos;
    list_for_each(pos, &(g_forwards_config->root))
    {
        forward_set_t * fs = list_entry(pos, forward_set_t, next);
        region_set.insert(fs->region);
    }

    for (it=region_set.begin(); it!=region_set.end(); ++it)
    {
        DBG("tc_get_total_region_number it=%d",(*it));
    }

    DBG("tc_get_total_region_number size=%lu", region_set.size());

    return region_set.size();
}

/*
* return a proper server according to given region and asn.
*/
int
tc_get_ip_and_port_and_level(uint16_t region, uint16_t asn, uint32_t *ip,
        uint16_t *port, uint16_t *level)
{
    list_head_t *pos;
    list_for_each(pos, &(g_forwards_config->root))
    {
        forward_set_t * fs = list_entry(pos, forward_set_t, next);

        if ((region == fs->region) && (fs->topo_level != 0))
        {
            if(asn == fs->asn)
            {
                *ip = fs->start_ip; //TODO
                *port = 2935;
                *level = fs->topo_level;
                return 0;
            }
            else
            {
                /* return alias ip if there exist */
                const char * alias = tc_get_alias(fs->start_ip);
                if (alias)
                {
                    const char * s = alias, *e = alias;
                    while ((e = strchr(s, ';')) != NULL)
                    {
                        char str_ip[32] = {0};
                        uint32_t alias_ip;
                        uint16_t alias_asn=-1, alias_region=-1;
                        if (3 != sscanf(s, "%[^:]:%hu:%hu", str_ip, &alias_asn, &alias_region))
                        {
                            WRN("parse alias error.");
                            break;
                        }
                        INF("return alias: alias ip: %s, alias asn: %u, alias region: %u", str_ip, alias_asn, alias_region);
                        if (0 != util_str2ip(str_ip, &alias_ip))
                        {
                            ERR("convert string ip to int ip error.");
                            break;
                        }

                        if(asn == alias_asn)
                        {
                            *ip = alias_ip;
                            *port = 2935;
                            *level = fs->topo_level;
                            INF("returned alias ip");
                            return 0;
                        }
                        s = e+1;
                        if (*s == '\0')
                            break;
                    }
                }
            }
        }
    }

    return -1;
}

static void show_help()
{
    /* TODO */
    fprintf(stderr, "%s\n", "./tracker -c /path/to/configfile");
}

int tracker_parse_arg(int argc, char **argv)
{
    if (!g_config)
    {
        ERR("calloc tracker config struct error");
        return -1;
    }

    g_config->listen_port = 1841;

    g_config->is_daemon = 1;
    g_config->child_topo_level = -1;

    char c;
    while(-1 != (c = getopt(argc, argv, "hp:H:Dvc:")))
    {
        switch(c)
        {
        case 'D':
            g_config->is_daemon = 0;
            break;
        case 'p':
            g_config->listen_port = atoi(optarg);
            break;
        case 'H':
            snprintf(g_config->listen_ip, strlen(optarg)+1, "%s", optarg);
            break;
        case 'c':
            snprintf(g_config->configfile, strlen(optarg)+1, "%s", optarg);
            break;
        case 'v':
            fprintf(stderr, "tracker %s\n", TRACKER_VERSION);
            fprintf(stderr, "\tbuild %s\n", TRACKER_VERSION_BUILD);
            fprintf(stderr, "\tbranch %s\n", TRACKER_VERSION_GIT_BRANCH);
            fprintf(stderr, "\tsha %s\n", TRACKER_VERSION_GIT_SHA);
            fprintf(stderr, "\tbuilt %s\n", TRACKER_VERSION_DATE);
            exit(0);
        case 'h':
        default:
            show_help();
            exit(0);
            //return 1;
        }
    }

    return 0;
}

const char *
tc_get_alias(uint32_t ip)
{
    list_head_t *pos;
    list_for_each(pos, &(g_group_config->root))
    {
        group_t * node = list_entry(pos, group_t, next);
        if (ip == node->lead_ip)
            return node->alias;
    }
    return NULL;
}


int tc_get_all_alias(uint32_t ip, char *alias[])
{
    int alias_cnt = 0;
    list_head_t *pos;

    list_for_each(pos, &(g_group_config->root))
    {
        group_t * node = list_entry(pos, group_t, next);
        if (ip == node->lead_ip)
        {
            if (NULL != alias)
            {
                strncpy(alias[alias_cnt], node->alias, 32);
            }
            alias_cnt++;
        }
    }

    return alias_cnt;
}
