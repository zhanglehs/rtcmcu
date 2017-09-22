#pragma once

#include <stdint.h>

#define STREAM_ID_LEN 16

#pragma pack(1)

enum RTPProtoType
{
    PROTO_RTP = 0,
    PROTO_RTCP = 1,
    PROTO_RTP_STREAMID = 2,
    PROTO_RTCP_STREAMID = 3,
    PROTO_CMD = 254,
};

typedef struct rtp_common_header
{
    uint8_t flag;
    uint8_t rtp_proto_type;
    uint16_t payload_len;
}rtp_common_header;

typedef struct rtp_proto_header
{
    rtp_common_header common_header;
    uint8_t payload[0];
}rtp_proto_header;


typedef struct rtp_streamid_proto_header
{
    rtp_common_header common_header;
    uint8_t streamid[STREAM_ID_LEN];
    uint8_t payload[0];
}rtp_streamid_proto_header;

typedef struct rtp_cmd_proto_header
{
    rtp_common_header common_header;
    uint8_t payload[0];
}rtp_cmd_proto_header;

// rtp uploader <----> receiver
// CMD_RTP_U2R_REQ_STATE = 500,

typedef struct rtp_u2r_req_state
{
    uint32_t version;
    uint8_t streamid[STREAM_ID_LEN];
    uint64_t user_id;
    uint8_t token[32];
    uint8_t payload_type;
}rtp_u2r_req_state;

// CMD_RTP_U2R_RSP_STATE = 501,
typedef struct rtp_u2r_rsp_state
{
    uint32_t version;
    uint8_t streamid[STREAM_ID_LEN];
    uint16_t result; //0 success; 1 token invalid; 2 streamid invalid;
}rtp_u2r_rsp_state;

// 502
typedef struct rtp_u2r_packet_header
{
    uint8_t streamid[STREAM_ID_LEN];
}rtp_u2r_packet_header;

// 503
typedef struct rtcp_u2r_packet_header
{
    uint8_t streamid[STREAM_ID_LEN];
}rtcp_u2r_packet_header;

// rtp downloader <----> player
// CMD_RTP_D2P_REQ_STATE = 600,

typedef struct rtp_d2p_req_state
{
    uint32_t version;
    uint8_t streamid[STREAM_ID_LEN];
    uint8_t token[32];
    uint8_t payload_type;
    uint8_t useragent[128];
}rtp_d2p_req_state;

// CMD_RTP_D2P_RSP_STATE = 601,
typedef struct rtp_d2p_rsp_state
{
    uint32_t version;
    uint8_t streamid[STREAM_ID_LEN];
    uint16_t result; //0 success; 1 token invalid; 2 streamid invalid;
}rtp_d2p_rsp_state;

// 602
typedef struct rtp_d2p_packet_header
{
    uint8_t streamid[STREAM_ID_LEN];
}rtp_d2p_packet_header;

// 603
typedef struct rtcp_d2p_packet_header
{
    uint8_t streamid[STREAM_ID_LEN];
}rtcp_d2p_packet_header;

// rtp forward <----> forward
// CMD_RTP_F2F_REQ_STATE = 700,

typedef struct rtp_f2f_req_state
{
    uint32_t version;
    uint8_t streamid[STREAM_ID_LEN];
}rtp_f2f_req_state;

struct f2f_rtp_packet_header {
    uint8_t sid[STREAM_ID_LEN];
    uint8_t payload_type; // RTPProtoType
};

struct f2f_rtp_result : public f2f_rtp_packet_header
{
    uint16_t result;
};

#pragma pack()
