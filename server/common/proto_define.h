#pragma once

#ifdef WIN32
typedef char                int8_t;
typedef unsigned char       uint8_t;
typedef short               int16_t;
typedef unsigned short      uint16_t;
typedef int                 int32_t;
typedef unsigned int        uint32_t;
typedef __int64             int64_t;
typedef unsigned __int64    uint64_t;
#else
#include <stdint.h>
#endif

typedef uint16_t proto_t;
typedef uint16_t f2t_v2_seq_t;
#define STREAM_ID_LEN 16

#include <sys/time.h>

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

static const int32_t TRACKER_RSP_RESULT_OK = 0;
static const int32_t TRACKER_RSP_RESULT_BADREQ = 1;
static const int32_t TRACKER_RSP_RESULT_NORESULT = 2;
static const int32_t TRACKER_RSP_RESULT_INNER_ERR = 3;

static const int32_t FORWARD_RSP_RESULT_OK = 0;
static const int32_t FORWARD_RSP_RESULT_BADREQ = 1;
static const int32_t FORWARD_RSP_RESULT_NORESULT = 2;
static const int32_t FORWARD_RSP_RESULT_INNER_ERR = 3;

static const int32_t FORWARD_SESSION_PROG_START = 0;
static const int32_t FORWARD_SESSION_PROG_IN_PROGRESS = 1;
static const int32_t FORWARD_SESSION_PROG_NOT_DATA = 2;
static const int32_t FORWARD_SESSION_PROG_BADREQ = 3;
static const int32_t FORWARD_SESSION_PROG_INNER_ERR = 4;
static const int32_t FORWARD_SESSION_PROG_WAIT = 5;//Need to wait for the data
static const int32_t FORWARD_SESSION_PROG_END = 6;

typedef enum
{
  UPD_CMD_DEL = 0,
  UPD_CMD_ADD
}update_cmd;

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

#pragma pack()

struct proto_wrapper
{
  proto_header* header;
  void* payload;
};
