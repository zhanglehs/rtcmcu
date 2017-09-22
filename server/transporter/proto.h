#ifndef PROTO_H
#define PROTO_H

#include <stdint.h>

#pragma pack(push,1) /* set alignment to byte */

struct stream_info
{
    uint64_t uuid;

    /* all variables below are in network order */
    uint16_t streamid;
    uint16_t bps;
    uint32_t ip;
    uint16_t port;

    /* new variables, added at 11/03/2009 */

    uint16_t buffer_minutes; /* buffer time in minute */

    uint8_t max_ts_count; /* max ts number, 0 -> disable ts */
    uint8_t max_depth; /* channel's max depth, decrease 1 during every transporter, 0 -> disable channel */
    uint8_t reserved_3; /* reserved 3 */
    uint8_t reserved_4; /* reserved 4 */
};

struct proto_header
{
    char magic[2];
    /* all variables below are in network order */
    uint16_t length; /* length of packet including proto_header */
    uint8_t method;
    uint8_t reserved; /* protocol reserved */
    uint16_t stream_count; /* stream count for LISTALL */
};

/* Protocol Magic String */
#define PROTO_MAGIC "YK"

/* methods */
#define LIST_ALL_CMD 1
#define STOP_ALL_CMD 2
#define START_ALL_CMD 3
#define CLEAR_ALL_CMD 4 /* clear all stream structures */
#define RESET_CMD 5 /* reset single stream */

struct report
{
    uint32_t code;
    uint32_t ip;
    uint32_t port;
    uint32_t stream;
    uint32_t bps;
    uint32_t action;
    uint32_t ts;

    uint64_t sid;
    uint32_t res; /* reserved */
};

#define REPORT_MAGIC_CODE 0x84F4613F
#define REPORT_JOIN 1
#define REPORT_LEAVE 2
#define REPORT_KEEPALIVE 3

#define P2P_IS_OK 0x1 /* is continuous to go P2P network*/
#define P2P_HAS_KEYFRAME 0x2
#define P2P_HAS_AVC0 0x4
#define P2P_HAS_AAC0 0x8
#define P2P_HAS_METADATA 0x10
#define P2P_HAS_HOLE 0x20
#define P2P_MP3_AUDIO 0x40

struct p2p_header
{
    char magic[2]; /* 'PL' */
    uint32_t packet_number; /* id by seconds */
    uint32_t length; /* length of p2p packet, including p2p header */
    uint32_t crc32;
    uint16_t duration; /* in ms */
    char flag; /* P2P FLAGS */
    char ver; /* version of p2p packet, current is 1 */
    char unused[2]; /* reservered */
};
/* restore to previous saved alignment */
#pragma pack(pop)

#endif
