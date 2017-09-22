/*****************************************************************************

 * tsmux.h: 

 *****************************************************************************

 * Copyright (C) 2005-2012 Tudou 

 *

 * Authors: Hualong Jiao <hljiao@tudou.com>

 *****************************************************************************/

#ifndef _TSMUX_H_

#define _TSMUX_H_

#include <stdint.h>

#if (defined __cplusplus && !defined _WIN32)
extern "C"
{
#endif
/* pids */

#define PAT_PID                 0x0000

#define SDT_PID                 0x0011

/* table ids */

#define PAT_TID   0x00

#define PMT_TID   0x02

#define SDT_TID   0x42

#define STREAM_TYPE_PRIVATE_SECTION 0x05

#define STREAM_TYPE_PRIVATE_DATA    0x06

#define STREAM_TYPE_AUDIO_AAC       0x0f

#define STREAM_TYPE_VIDEO_H264      0x1b

#define VIDEO_STREAM_ID             0xe0

#define AUDIO_STREAM_ID             0xc0

#define VIDEO_PACKET_PID            0x0100

#define AUDIO_PACKET_PID            0x0101

#define PMT_PACKET_PID              0x0fff

#define MEMORY_UNIT_SIZE            (512 * 1024)

#define       VIDEO_STREAM_DESCRIPTOR                     2

#define       AUDIO_STREAM_DESCRIPTOR                     3

#define       HIERARCHY_DESCRIPTOR                        4

#define       REGISTRATION_DESCRIPTOR                     5

#define       DATA_STREAM_ALIGNMENT_DESCRIPTOR            6

#define       TARGET_BACKGROUND_GRID_DESCRIPTOR           7

#define       VIDEO_WINDOW_DESCRIPTOR                     8

#define       CA_DESCRIPTOR                               9

#define       ISO_639_LANGUAGE_DESCRIPTOR                 10

#define       SYSTEM_CLOCK_DESCRIPTOR                     11

#define       MULTIPLEX_BUFFER_UTILIZATION_DECSRIPTOR     12

#define       COPYRIGHT_DESCRIPTOR                        13

#define       PRIVATE_DATA_INDICATOR_DESCRIPTOR           15

#define       SMOOTHING_BUFFER_DESCRIPTOR                 16

#define       IBP_DESCRIPTOR                              18

#define       DEBUG_PRINTF                                0

typedef struct ts_packet
{

    unsigned short sync_byte:8;

    unsigned short transport_error_indicator:1;

    unsigned short payload_unit_start_indicator:1;

    unsigned short transport_priority:1;

    unsigned short PID:13;

    unsigned short transport_scrambling_control:2;

    unsigned short adaptation_field_control:2;

    unsigned short continuity_counter:4;

} TS_PACKET;

typedef struct ts_packet_extention_header
{

    unsigned short sync_byte:8;

    unsigned short transport_error_indicator:1;

    unsigned short payload_unit_start_indicator:1;

    unsigned short transport_priority:1;

    unsigned short PID:13;

    unsigned short transport_scrambling_control:2;

    unsigned short adaptation_field_control:2;

    unsigned short continuity_counter:4;

    unsigned short adaptation_field_length:8;

    unsigned short discontinuity_indicator:1;

    unsigned short random_access_indicator:1;

    unsigned short elementary_stream_priority_indicator:1;

    unsigned short PCR_flag:1;

    unsigned short OPCR_flag:1;

    unsigned short splicing_point_flag:1;

    unsigned short transport_private_data_flag:1;

    unsigned short adaptation_field_extension_flag:1;

} TS_PACKET_EXTENTION;

typedef struct pat_entry
{

    unsigned short program_number;  //2 bytes

    unsigned short reserved:3;

    unsigned short PID:13;

} PAT_ENTRY;

typedef struct pat_section_header
{

    unsigned short table_id:8;

    unsigned short section_syntax_indicator:1;

    unsigned short zero:1;

    unsigned short reserved1:2;

    unsigned short section_length:12;

    unsigned short transport_stream_id:16;

    unsigned short reserved2:2;

    unsigned short version_number:5;

    unsigned short current_next_indicator:1;

    unsigned short section_number:8;

    unsigned short last_section_number:8;

    PAT_ENTRY *pat_entry;

    unsigned short pat_entry_number;

} PAT_SECTION_HEADER;

typedef struct pmt_section
{

    unsigned short table_id:8;

    unsigned short section_syntax_indicator:1;

    unsigned short zero:1;

    unsigned short reserved1:2;

    unsigned short section_length:12;

    unsigned short program_number:16;

    unsigned short reserved2:2;

    unsigned short version_number:5;

    unsigned short current_next_indicator:1;

    unsigned short section_number:8;

    unsigned short last_section_number:8;

    unsigned short reserved3:3;

    unsigned short PCR_PID:13;

    unsigned short reserved4:4;

    unsigned short program_info_length:12;

    unsigned short stream_number;

} PMT_SECTION;

typedef struct stream_descriptor
{

    unsigned short stream_type:8;

    unsigned short reserved1:3;

    unsigned short elementary_PID:13;

    unsigned short reserved2:4;

    unsigned short ES_info_length:12;

    char descriptor[512];

} STREAM_DESCRIPTOR;

typedef struct video_stream_descriptor
{

    unsigned short descriptor_tag:8;

    unsigned short descriptor_length:8;

    unsigned short multiple_frame_rate_flag:1;

    unsigned short frame_rate_code:4;

    unsigned short MPEG_1_only_flag:1;

    unsigned short constrained_parameter_flag:1;

    unsigned short still_picture_flag:1;

    //if (MPEG_1_only_flag == 1)

    unsigned short profile_and_level_indication:8;

    unsigned short chroma_format:2;

    unsigned short frame_rate_extension_flag:1;

    unsigned short reserved:5;

} V_STREAM_DESCRIPTOR;

typedef struct audio_stream_descriptor
{

    unsigned short descriptor_tag:8;

    unsigned short descriptor_length:8;

    unsigned short free_format_flag:1;

    unsigned short ID:1;

    unsigned short layer:2;

    unsigned short variable_rate_audio_indicator:1;

    unsigned short reserved:3;

} A_STREAM_DESCRIPTOR;

typedef struct ts_init_parameter
{

    unsigned int aud_stream_id;

    unsigned int vid_stream_id;

    unsigned int pcr_pid;

    unsigned int vid_pkt_pid;

    unsigned int aud_pkt_pid;

    unsigned int pmt_pid;

    unsigned int transportId;

    unsigned int bfirstInit;

} TS_INIT_PARA;

int mpegts_init(void **tsmux_handle, TS_INIT_PARA * pTsInitPara,
                int bVideoflag, int bAudioflag);

int mpegts_reset(void *tsmux_handle);

int mpegts_buf_reset(void *tsmux_handle);

int mpegts_close(void *tsmux_handle);

int mpegts_write_pat(void *tsmux_handle);

int mpegts_write_pes_with_pcr(void *tsmux_handle, char *payload,
                              int payload_size, int stream_id, int keyframe,
                              uint64_t pcr_base, unsigned int pcr_extention);

int mpegts_write_pes(void *tsmux_handle, char *payload, int payload_size,
                     int stream_id, int keyframe);

int mpegts_write_pmt(void *tsmux_handle);

int mpegts_write_dummy(void *tsmux_handle);

int mpegts_get_buffer(void *tsmux_handle, unsigned char **outputBuffer);

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif /* 
        */
