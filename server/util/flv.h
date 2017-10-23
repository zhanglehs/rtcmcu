/*******************************************
 * YueHonghui, 2013-05-20
 * hhyue@tudou.com
 * copyright:youku.com
 * ***************************************/

#ifndef FLV_H_
#define FLV_H_
#include <stdint.h>
#include "common.h"
#include "../util/buffer.hpp"

#if (defined __cplusplus && !defined _WIN32)
extern "C"
{
#endif

#define FLV_AUDIO_HDR "\x46\x4c\x56\x01\x04\x00\x00\x00\x09"

enum
{
    FLV_TAG_AUDIO = 0x08,
    FLV_TAG_VIDEO = 0x09,
    FLV_TAG_SCRIPT = 0x12,
};

enum
{
    FLV_FLAG_VIDEO = 1,
	FLV_FLAG_WAIT_VKEY =3,
    FLV_FLAG_AUDIO = 4,
    FLV_FLAG_BOTH = 5,
};

enum
{
    FLV_KEYFRAME = 0x01,
    FLV_INTER = 0x02,
    FLV_DISPOSABLE = 0x03,
    FLV_GEN_KEY = 0x04,
    FLV_INFO = 0x05,
};

enum
{
    AUDIO_MP3 = 0x02,
    AUDIO_AAC = 0x0A,
    AUDIO_8KMP3 = 0x0E,
};

enum
{
    VIDEO_VP6 = 0x04,
    VIDEO_VP6_ALPHA = 0x05,
    VIDEO_AVC = 0x07,
};

enum
{
    AVC_CONFIG = 0x00,
    AVC_NALU = 0x01,
    AVC_EOS = 0x02,
};

enum
{
    AAC_CONFIG = 0x00,
    AAC_RAW = 0x01,
};

#pragma pack(1) 
typedef struct flv_header
{
    uint8_t signature[3];
    uint8_t version;
    uint8_t flags;
    uint8_t offset[4];
    uint8_t data[0];
} flv_header;

typedef struct flv_tag
{
    uint8_t type;
    uint8_t datasize[3];
    uint8_t timestamp[4];
    uint8_t streamid[3];
    uint8_t data[0];
} flv_tag;

typedef struct flv_tag_video
{
    struct
    {
        uint8_t codecid:4;
        uint8_t frametype:4;
    } video_info;
    uint8_t avc_packet_type;
    uint8_t cts[3];
    uint8_t data[0];
} flv_tag_video;

typedef struct flv_tag_audio
{
    struct
    {
        uint8_t sound_type:1;
        uint8_t sound_sz:1;
        uint8_t sound_rate:2;
        uint8_t sound_fmt:4;
    } audio_info;
    uint8_t data[0];
} flv_tag_audio;

#pragma pack()

boolean flv_is_bframe(const flv_tag_video*);

static inline uint32_t
flv_get_header_offset(uint8_t * offset)
{
    return (((((offset[0] << 8) + offset[1]) << 8) + offset[2]) << 8) +
        offset[3];
}

static inline boolean
flv_is_aac0(const void *ptr)
{
    const flv_tag_audio *fta = (const flv_tag_audio *) ptr;

    if(AUDIO_AAC == fta->audio_info.sound_fmt && AAC_CONFIG == fta->data[0])
        return TRUE;
    return FALSE;
}

static inline boolean
flv_is_avc0(const void *ptr)
{
    const flv_tag_video *ftv = (const flv_tag_video *) ptr;

    if(VIDEO_AVC == ftv->video_info.codecid
       && AVC_CONFIG == ftv->avc_packet_type)
        return TRUE;
    return FALSE;
}

static inline uint32_t
flv_get_timestamp(const uint8_t * ts)
{
    return (ts[3] << 24) | (ts[0] << 16) | (ts[1] << 8) | ts[2];
}

static inline void
flv_set_timestamp(uint8_t * ts, uint32_t value)
{
    uint8_t *vstart = (uint8_t *) & value;

    ts[3] = vstart[3];
    ts[2] = vstart[0];
    ts[1] = vstart[1];
    ts[0] = vstart[2];
}

static inline uint32_t
flv_get_datasize(const uint8_t * datasz)
{
    return datasz[0] << 16 | datasz[1] << 8 | datasz[2];
}

static inline uint32_t
flv_get_pretagsize(const uint8_t * datasz)
{
    return datasz[0] << 24 | datasz[1] << 16 | datasz[2] << 8 | datasz[3];
}

static inline void
flv_set_datasize(uint8_t * datasz, int32_t value)
{
    datasz[0] = (value >> 16) & 0xff;
    datasz[1] = (value >> 8) & 0xff;
    datasz[2] = value & 0xff;
}

static inline void
flv_set_pretagsize(uint8_t * datasz, int32_t value)
{
    datasz[0] = (value >> 24) & 0xff;
    datasz[1] = (value >> 16) & 0xff;
    datasz[2] = (value >> 8) & 0xff;
    datasz[3] = value & 0xff;
}

static inline uint32_t
FLV_UI16(const uint8_t * buf)
{
    return (buf[0] << 8) + buf[1];
}

static inline uint32_t
FLV_UI24(const uint8_t * buf)
{
    return (((buf[0] << 8) + buf[1]) << 8) + buf[2];
}

static inline uint32_t
FLV_UI32(const uint8_t * buf)
{
    return (((((buf[0] << 8) + buf[1]) << 8) + buf[2]) << 8) + buf[3];
}

typedef struct flv_tag_iterator
{
    uint8_t *data_ptr;
    size_t size;
    size_t next_pos;
} flv_tag_iterator;

static inline void
flv_tag_iterator_init(flv_tag_iterator * iterator, void *ptr, size_t size)
{
    iterator->data_ptr = (uint8_t *) ptr;
    iterator->size = size;
    iterator->next_pos = 0;
}

static inline boolean
flv_tag_iterate(flv_tag_iterator * iterator, flv_tag ** tag)
{
    if(iterator->next_pos >= iterator->size)
        return FALSE;
    *tag = (flv_tag *) (iterator->data_ptr + iterator->next_pos);
    iterator->next_pos +=
        sizeof(flv_tag) + flv_get_datasize((*tag)->datasize) + 4;
    if(iterator->next_pos > iterator->size)
        return FALSE;
    return TRUE;
}

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
