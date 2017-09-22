#ifndef TRANSPORTER_H
#define TRANSPORTER_H

#include <stdio.h>
#include <arpa/inet.h>
#include <event.h>

#include "buffer.h"
#include "proto.h"

/*
 * <config>
 *  <port>81</port>
 *  <maxconns>4096</maxconns>
 *  <upstream>ip:port</upstream>
 *  <bind>0.0.0.0</bind>
 *  <localip>internal ip address</localip>
 * </config>
 */
struct config
{
    /* server */
    int maxfds;

    int port;
    char bindhost[33];

    char stream_ip[33];

    char admin_ip[33];
    int admin_port;

    FILE *logfp;

    int live_buffer_time;

    struct sockaddr_in supervisor_server;

    /* report */
    int report_interval;
    int report_fd;
    struct sockaddr_in report_server;

    /* max lag seconds between current stream timestamp */
    int client_max_lag_seconds;

    int seek_to_keyframe;
    int client_write_blocksize;
    int client_expire_time;
    int max_backend_retry_count;
};

struct stream_data;
struct connection;

/* H264 FLV */
struct ts_flv_param
{
    int sps_len;
    unsigned char* sps;
    int pps_len;
    unsigned char* pps;

    int objecttype;
    int sample_rate_index;
    int channel_conf;
};

struct ts_chunkqueue
{
    struct buffer *mem;
    time_t ts;
    time_t duration;
    uint32_t squence;
    struct ts_chunkqueue *next;
};

struct connection_list
{
    struct connection **conns;
    int used, size;
};

typedef enum
{
    state_start = 0,
    state_http_header,
    state_flv_header,
    state_flv_tag,
    state_flv_data
} live_state_t;

struct stream
{
    struct stream_info si;
    uint8_t max_ts_count;

    struct segment *head, *tail;

    uint64_t start_ts; /* this stream's start time, in ms */
    uint32_t last_stream_ts; /* last relative timestamp */

    /* onMetaData buffer */
    buffer *onmetadata;

    /* AVC Seq Tag */
    buffer *avc0;

    /* AAC Seq Tag */
    buffer *aac0;

    struct connection_list *waitqueue, *waitqueue_bak;
    int connected;

    /* live connection variables */
    int fd;
    live_state_t state;
    struct event ev;

    buffer *r;
    buffer *w;

    int wpos, tagsize;

    uint64_t backend_start_ts; /* this live stream connection's start_ts in onMetaData*/
    int32_t ts_diff;

    int retry_count;

    int width, height, framerate, audiosamplerate;
    double video_interval, audio_interval;

    uint32_t live_buffer_time;

    int switch_backend;
    
    /* ts info */
    struct ts_flv_param g_param;
    struct buffer *ipad_mem;;
    time_t ipad_start_ts, ipad_end_ts;
    uint32_t ts_squence, ts_reset_ticker;
    struct ts_chunkqueue *ipad_head, *ipad_tail;
    uint32_t cq_number, max_ts_duration;
    uint32_t g_video_c, g_audio_c;
};

typedef struct
{
    int key;
    char *value;
} keyvalue;

typedef enum
{
    conn_read = 0,
    conn_write /* write live flash stream */
} conn_state_t;

#define LIVE_MODE 0
#define TS_MODE 1
#define M3U8_MODE 2
#define P2P_MODE 3

typedef struct connection conn;

/* client connection structure */
struct connection
{
    int fd;
    conn_state_t state;
    struct event ev;

    char remote_ip[20];
    int remote_port;

    char local_ip[20];
    int local_port;

    /* http request header */
    int is_first_line;

    int streamid; /* 1~4 */
    uint32_t bps; /* 50, 200, 400 */
    uint64_t sid; /* session id */
    int64_t offset; /* live start second, 0 is current */

    /* http response */
    int http_status;

    buffer *r, *w;

    int wpos;

    struct stream *s;

    struct segment *live;

    uint32_t live_start, live_end;
    time_t live_end_ts;

    int in_waitqueue;

    time_t report_ts;

    int seek_to_keyframe;
    int is_crossdomainxml;

    time_t expire_ts;

    int is_writable;

    int stream_mode; /* 0 -> Live, 1 -> TS, 2 -> M3U8, 3 -> P2P */
    int to_close; /* 1 -> close after writing w buffer */

    /* P2P variables */
    time_t p2p_ts;
    time_t p2p_duration; /* in ms */
    uint8_t p2p_flag;
    uint8_t p2p_next;

    /* from p2p request */
    int blocks; /* -1 means forever, >0 means after send n blocks and close */

};

extern time_t cur_ts;
extern char cur_ts_str[128];

/* error log define */
#define ERROR_LOG(fp, fmt, args...) \
    do {fprintf(fp, "%s: (%s.%d) " fmt, cur_ts_str, __FILE__, __LINE__, ##args); } while(0)

#endif
