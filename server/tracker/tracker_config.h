/*
 * author: hechao@youku.com
 * create: 2013.6.17
 */

#ifndef TRACKER_CONFIGURE_H
#define TRACKER_CONFIGURE_H

#include <stdint.h>
#include <set>
#include <util/list.h>

#define DIR_NAME_LEN 256
#define FILE_NAME_LEN 256
#define IP_LEN 32

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct tracker_config
{
    char listen_ip[16];
    uint16_t listen_port;
    char portal_ip[16];
    uint16_t portal_port;
    uint16_t notifier_listen_port;
    uint16_t http_listen_port;

    char log_dir[DIR_NAME_LEN];
    char log_name[FILE_NAME_LEN];
    char log_svr_ip[IP_LEN];
    uint16_t log_svr_port;
    int  log_level;

    /* the forwards whose topo_level equals to or more than child_topo_level
     * will not be added to stream forward hierachy actually for reducing
     * the scale of forwards that under management of forward_manager
     */
    uint8_t  child_topo_level;
    char configfile[FILE_NAME_LEN];

    int32_t is_daemon;
}tracker_config_t;

extern tracker_config_t *g_config;

typedef struct forward_set
{
    uint32_t start_ip;
    uint32_t stop_ip;
    uint16_t asn;
    uint16_t region;
    uint8_t topo_level;
    list_head_t next;
}forward_set_t;

typedef struct forwards_config
{
    list_head_t root;
}forwards_config_t;

#define MEMBERS_LEN 2048
typedef struct group
{
    uint32_t lead_ip;
    char alias[MEMBERS_LEN];

    list_head_t next;
}group_t;

typedef struct group_config
{
    list_head_t root;
}group_config_t;

int tracker_config_init();
int tracker_parse_arg(int argc, char **argv);
int tracker_parse_config();

int tc_get_asn_and_region_and_level(uint32_t ip, uint16_t *asn, uint16_t *region, uint8_t *level);
int tc_get_total_region_number();
int tc_get_ip_and_port_and_level(uint16_t region, uint16_t asn, uint32_t *ip, uint16_t *port, uint16_t *level);

const char *tc_get_alias(uint32_t ip);
int tc_get_all_alias(uint32_t ip, char *alias[]);

#ifdef __cplusplus
}
#endif
#endif

