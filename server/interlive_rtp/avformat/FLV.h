/**
* @file
* @brief
* @author   songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2015/07/24
* @see
*/



#ifndef __FLV_H_
#define __FLV_H_

#include <stdint.h>
#include "utils/buffer.hpp"
#include "avcodec/h264.h"
#include "avcodec/aac.h"
#include "../util/flv.h"

enum {
	FLV_VIDEO_JPEG = 0x01,
	FLV_VIDEO_H263 = 0x02,
	FLV_VIDEO_SCREEN_VIDEO = 0X03,
	FLV_VIDEO_VP6 = 0x04,
	FLV_VIDEO_VP6A = 0x05,
	FLV_VIDEO_SCREEN_VIDEO2 = 0x06,
	FLV_VIDEO_AVC = 0x07
};

enum 
{
	FLV_AUDIO_PCM = 0x00,
	FLV_AUDIO_ADPCM = 0x01,
	FLV_AUDIO_MP3 = 0x02,
	FLV_AUDIO_PCM_LE = 0x03,
	FLV_AUDIO_NELLY_MOSER_16K = 0x04,
	FLV_AUDIO_NELLY_MOSER_8K = 0x05,
	FLV_AUDIO_NELLY_MOSER = 0x06,
	FLV_AUDIO_PCM_ALAW = 0x07,
	FLV_AUDIO_PCM_MULAW = 0x08,
	FLV_AUDIO_RESERVED = 0x09,
	FLV_AUDIO_AAC = 0x0a,
	FLV_AUDIO_SPEEX = 0x0b,
	FLV_AUDIO_MP3_8K = 0x0e,
	FLV_AUDIO_DEVICE_SPECIFIC = 0x0f
};

class FLVHeader
{
public:
    FLVHeader();
    ~FLVHeader();

    int build(const uint8_t *avc_config, uint32_t avc_config_len,
        const uint8_t* aac_config, int32_t aac_config_len,
        int32_t& status_code);

    int generate_aac0_tag(const uint8_t *payload, uint32_t payload_len,
        flv_tag* tag, int32_t& tag_len, int32_t& status);

    int generate_avc0_tag(const uint8_t *payload, uint32_t payload_len,
        flv_tag* tag, int32_t& tag_len, int32_t& status);

    int generate_metadata_tag(
        const uint8_t *avc_config, uint32_t avc_config_len,
        const uint8_t* aac_config, int32_t aac_config_len,
        flv_tag* tag, int32_t& tag_len, int32_t& status);

    int generate_aac0_data(const uint8_t* aac_config, int32_t aac_config_len,
        uint8_t* output, int32_t& output_len, int32_t& status);

    int generate_avc0_data(const uint8_t *avc_config, uint32_t avc_config_len,
        uint8_t* output, int32_t& output_len, int32_t& status);

    char* EncMetaData(char *enc, char *pend, bool bFLVFile,int width,int height,int samplerate,int channel);

    flv_header* get_header(int32_t& len);

private:
    flv_header* _full_header;

    uint8_t _flv_flag;
    buffer* _buffer;
};

class FLV
{
public:
    FLV();

    ~FLV();

    flv_tag* generate_audio_tag(
        const uint8_t* aac_payload, uint32_t aac_payload_len,
        int32_t timestamp,
        int32_t& tag_len, int32_t& status);

    int32_t generate_audio_tag_data(const uint8_t *payload, uint32_t payload_len,
        uint8_t* tag_data, int32_t& data_len, int32_t& status);

    flv_tag* generate_video_tag(
        const uint8_t *payload, uint32_t payload_len,
        int32_t timestamp,
        int32_t& tag_len, int32_t& status);

    int32_t generate_video_tag_data(const uint8_t *payload, uint32_t payload_len,
        uint8_t* tag_data, int32_t& data_len, int32_t& status);

    //static x264_nal_t* get_x264_nal(flv_tag* tag, int32_t& nal_num);

    //static uint8_t* get_aac_payload(flv_tag* tag, int32_t& len);

    static inline int32_t flv_get_timestamp(const uint8_t * ts)
    {
        return (ts[3] << 24) | (ts[0] << 16) | (ts[1] << 8) | ts[2];
    }

    static inline uint32_t flv_get_datasize(const uint8_t * datasz)
    {
        return datasz[0] << 16 | datasz[1] << 8 | datasz[2];
    }

    static inline uint32_t flv_get_pretagsize(const uint8_t * datasz)
    {
        return datasz[0] << 24 | datasz[1] << 16 | datasz[2] << 8 | datasz[3];
    }

    static inline void flv_set_pretagsize(uint8_t * datasz, int32_t value)
    {
        datasz[0] = (value >> 24) & 0xff;
        datasz[1] = (value >> 16) & 0xff;
        datasz[2] = (value >> 8) & 0xff;
        datasz[3] = value & 0xff;
    }

    static inline void flv_set_timestamp(uint8_t * ts, int32_t value)
    {
        uint8_t *vstart = (uint8_t *)& value;

        ts[3] = vstart[3];
        ts[2] = vstart[0];
        ts[1] = vstart[1];
        ts[0] = vstart[2];
    }

    static  inline void flv_set_datasize(uint8_t * datasz, int32_t value)
    {
        datasz[0] = (value >> 16) & 0xff;
        datasz[1] = (value >> 8) & 0xff;
        datasz[2] = value & 0xff;
    }

    static  inline void flv_set_streamid(uint8_t * streamid, int32_t value)
    {
        streamid[0] = (value >> 16) & 0xff;
        streamid[1] = (value >> 8) & 0xff;
        streamid[2] = value & 0xff;
    }

    static inline bool flv_is_aac0(const void *ptr)
    {
        const flv_tag_audio *fta = (const flv_tag_audio *)ptr;

        if (FLV_AUDIO_AAC == fta->audio_info.sound_fmt && AAC_CONFIG == fta->data[0])
            return true;
        return false;
    }

    static inline bool flv_is_avc0(const void *ptr)
    {
        const flv_tag_video *ftv = (const flv_tag_video *)ptr;

        if (FLV_VIDEO_AVC == ftv->video_info.codecid
            && AVC_CONFIG == ftv->avc_packet_type)
            return true;
        return false;
    }
private:
    flv_tag* _temp_tag;
    int32_t _temp_tag_len;
    int32_t _initial_audio_ts;
    int32_t _initial_video_ts;

};

#endif /* __FLV_H_ */
