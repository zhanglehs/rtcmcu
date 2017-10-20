#pragma once

#include <stdint.h>

#define STREAM_ID_LEN 16

//#include <sys/time.h>

enum
{
  // rtp uploader <----> receiver
  CMD_RTP_U2R_REQ_STATE = 500,
  CMD_RTP_U2R_RSP_STATE = 501,
  CMD_RTP_U2R_PACKET = 502,
  CMD_RTCP_U2R_PACKET = 503,

  // rtp downloader <----> player
  CMD_RTP_D2P_REQ_STATE = 600,
  CMD_RTP_D2P_RSP_STATE = 601,
  CMD_RTP_D2P_PACKET = 602,
  CMD_RTCP_D2P_PACKET = 603,
};

typedef enum
{
  PAYLOAD_TYPE_MEDIA_BEGIN = 0,
  PAYLOAD_TYPE_FLV = 0,
  PAYLOAD_TYPE_MPEGTS = 1,

  PAYLOAD_TYPE_MEDIA_END = 31,
  PAYLOAD_TYPE_HEARTBEAT = 32,
}payload_type;

#pragma pack(1)
typedef struct proto_header
{
  uint8_t magic;
  uint8_t version;
  uint16_t cmd;
  uint32_t size;
}proto_header;

typedef struct media_header
{
  uint32_t streamid;
  uint32_t payload_size;
  uint8_t payload_type;
  uint8_t payload[0];
}media_header;

enum
{
  U2R_RESULT_SUCCESS = 0,
  U2R_RESULT_INVALID_TOKEN = 1,
  U2R_RESULT_INVALID_STREAMID = 2,
  U2R_RESULT_INVALID_IP = 3,
  U2R_RESULT_SYS_BUSY = 4,
};

enum RTPProtoType
{
  PROTO_RTP = 0,
  PROTO_RTCP = 1,
  PROTO_RTP_STREAMID = 2,
  PROTO_RTCP_STREAMID = 3,
  PROTO_CMD = 254,
};

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

#pragma pack()
