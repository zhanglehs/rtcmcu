/*******************************************************
 * YueHonghui, 2013-07-25
 * hhyue@tudou.com
 *
 * ported from jiaohualong
*******************************************************/


#include "flv2ts.h"
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "tsmux.h"
#include "utils/buffer.hpp"
#include "flv.h"

#define ADTS_HEADER_SIZE      7
#define MAX_PES_LENGTH       (0x00ffff+0x06)
#define TS_STREAM_ID          1
#define AUD_SAMPLE_RATE_NUM   9
#define AUDIO_FRAMES_PES      16
#define NON_KEYFRAME_SPLIT    1

typedef struct flv2ts_ctx
{
    flv2ts_para *paras;
    void *tshandle_ptr;

    uint8_t *aud_seq_hdr;
    size_t aud_seq_hdr_size;
    size_t aud_used_size;
    uint32_t aud_sample_rate;

    uint8_t *vid_seq_hdr;
    size_t vid_seq_hdr_size;
    size_t vid_used_size;

    buffer *aud_pes_buf;
    buffer *vid_pes_buf;
} flv2ts_ctx;

static void
close_ctx(flv2ts_ctx * ctx)
{
    mpegts_close(ctx->tshandle_ptr);
    if(ctx->aud_pes_buf) {
        buffer_free(ctx->aud_pes_buf);
    }
    if(ctx->vid_pes_buf) {
        buffer_free(ctx->vid_pes_buf);
    }
    free(ctx);
}

static int
convert_AVCDecoderConfiguration_to_annexb(uint8_t * avc_config,
                                          size_t avc_config_size,
                                          uint8_t * vid_seq_hdr_buf,
                                          size_t vid_seq_hdr_size,
                                          size_t * vid_used_size)
{
    unsigned char nSPS, nPPS;
    unsigned int length_SPS = 0, length_PPS = 0;
    unsigned char *ptr = vid_seq_hdr_buf;
    unsigned char *sps_buf = 0, *pps_buf = 0;
    unsigned int i;

    if (avc_config == NULL || avc_config_size == 0)
    {
        return -1;
    }

    assert(vid_seq_hdr_size >= avc_config_size);
    memcpy(vid_seq_hdr_buf, avc_config, avc_config_size);

    ptr[0] = ptr[1] = ptr[2] = 0x00;
    ptr[3] = 0x01;

    ptr += 16;

    nSPS = *(ptr + 5) & 0x1f;

    ptr += 6;

    for(i = 0; i < nSPS; i++) {
        length_SPS = *ptr;
        length_SPS <<= 8;
        length_SPS |= *(ptr + 1);

        sps_buf = ptr + 2;
        ptr += (length_SPS + 2);
    }

    nPPS = *(ptr) & 0xff;

    ptr++;

    for(i = 0; i < nPPS; i++) {
        length_PPS = *ptr;
        length_PPS <<= 8;
        length_PPS |= *(ptr + 1);

        pps_buf = ptr + 2;
        ptr += (length_PPS + 2);
    }
    ptr = vid_seq_hdr_buf;

    if(sps_buf) {
        memmove(ptr + 4, sps_buf, length_SPS);
    }

    ptr[4 + length_SPS] = ptr[5 + length_SPS] = ptr[6 + length_SPS] = 0x00;
    ptr[7 + length_SPS] = 0x01;

    if(pps_buf)
        memmove(ptr + 8 + length_SPS, pps_buf, length_PPS);

    *vid_used_size = 8 + length_SPS + length_PPS;

    return 0;
}

static int
convert_AACDecoderConfiguration_to_adts(const uint8_t * aac_config,
                                        size_t aac_config_size,
                                        uint8_t * aud_seq_hdr,
                                        size_t aud_seq_hdr_size,
                                        size_t * aud_used_size,
                                        uint32_t * aud_sample_rate)
{
    uint8_t profile_objecttype, sample_rate_index, channel_conf;

    if (aac_config == NULL || aac_config_size == 0)
    {
        return -1;
    }

    assert(aud_seq_hdr_size >= aac_config_size);

    //skip 13 bytes
    const uint8_t *ptr =
        aac_config + sizeof(flv_tag) + sizeof(flv_tag_audio) + 1;

    profile_objecttype = ((ptr[0] >> 3) & 0xff) - 1;
    sample_rate_index = ((ptr[0] & 0x07) << 1) | ((ptr[1] >> 7) & 0x01);
    channel_conf = ((ptr[1] >> 3) & 0x0f);

    if(0 == channel_conf) {
        profile_objecttype = 1;
        sample_rate_index = 7;
        channel_conf = 2;
    }
    aud_seq_hdr[0] = 0xff;
    aud_seq_hdr[1] = 0xf1;
    aud_seq_hdr[2] = (profile_objecttype & 0x03) << 6;
    aud_seq_hdr[2] |= (sample_rate_index & 0x0f) << 2;
    aud_seq_hdr[2] |= (channel_conf & 0x07) >> 2;
    aud_seq_hdr[3] = (channel_conf & 0x03) << 6;
    aud_seq_hdr[4] = 0x00;
    aud_seq_hdr[5] = 0x1f;
    aud_seq_hdr[6] = 0xfc;

    *aud_used_size = 7;

    /////////////////////////////////////////////

    if(sample_rate_index == 7)
        *aud_sample_rate = 22050;
    else if(sample_rate_index == 6)
        *aud_sample_rate = 24000;
    else if(sample_rate_index == 5)
        *aud_sample_rate = 32000;
    else if(sample_rate_index == 4)
        *aud_sample_rate = 44100;
    else if(sample_rate_index == 3)
        *aud_sample_rate = 48000;
    else if(sample_rate_index == 2)
        *aud_sample_rate = 64000;
    else if(sample_rate_index == 1)
        *aud_sample_rate = 96000;
    else if(sample_rate_index == 8)
        *aud_sample_rate = 16000;
    else if(sample_rate_index == 9)
        *aud_sample_rate = 12000;
    else if(sample_rate_index == 10)
        *aud_sample_rate = 11025;
    else if(sample_rate_index == 11)
        *aud_sample_rate = 8000;
    else
        *aud_sample_rate = 22050;

    /////////////////////////////////////////////
    return 0;
}

static int
is_vid_seq_hdr_added(flv_tag * tag)
{
    flv_tag_video *tv = (flv_tag_video *) tag->data;
    uint32_t datasize =
        flv_get_datasize(tag->datasize) - sizeof(flv_tag_video);
    uint8_t *vid_frame_ptr = tv->data;
    uint32_t frame_sz = 0, skip_sz = 0;

    frame_sz = FLV_UI32(vid_frame_ptr);
    if(skip_sz + 4 < datasize) {
        if(0x67 == vid_frame_ptr[skip_sz + 4])
            return 0;
    }
    while(skip_sz + frame_sz + 4 < datasize) {
        skip_sz += frame_sz + 4;
        frame_sz = FLV_UI32(vid_frame_ptr + skip_sz);
        if(skip_sz + 4 < datasize) {
            if(0x67 == vid_frame_ptr[skip_sz + 4])
                return 0;
        }
    }
    return 1;
}

static int
deal_vid_frame(uint8_t * frame_ptr, size_t size)
{
    uint32_t frame_sz = 0, skip_sz = 0;

    frame_sz = FLV_UI32(frame_ptr);
    frame_ptr[0] = frame_ptr[1] = frame_ptr[2] = 0x00;
    frame_ptr[3] = 0x01;
    while(skip_sz + frame_sz + 4 < size) {
        skip_sz += frame_sz + 4;
        frame_sz = FLV_UI32(frame_ptr + skip_sz);
        frame_ptr[skip_sz] =
            frame_ptr[skip_sz + 1] = frame_ptr[skip_sz + 2] = 0x00;
        frame_ptr[skip_sz + 3] = 0x01;
    }
    return 0;
}

int
flv2ts(const flv2ts_para * paras, buffer * obuf, int64_t * duration, uint8_t flv_tag_flag)
{
    flv2ts_ctx *ctx_ptr = 0;
    uint8_t aud_seq_hdr[1024 * 2];
    uint8_t vid_seq_hdr[1024 * 2];

    ctx_ptr = (flv2ts_ctx *) malloc(sizeof(flv2ts_ctx));
    if(NULL == ctx_ptr) {
        return -1;
    }
    memset(ctx_ptr, 0, sizeof(*ctx_ptr));

    ctx_ptr->paras = (flv2ts_para *) paras;
    ctx_ptr->tshandle_ptr = NULL;
    ctx_ptr->aud_seq_hdr = (uint8_t *) aud_seq_hdr;
    ctx_ptr->aud_seq_hdr_size = sizeof(aud_seq_hdr);
    ctx_ptr->aud_used_size = 0;
    ctx_ptr->aud_sample_rate = 0;
    ctx_ptr->vid_seq_hdr = (uint8_t *) vid_seq_hdr;
    ctx_ptr->vid_seq_hdr_size = sizeof(vid_seq_hdr);
    ctx_ptr->vid_used_size = 0;
    ctx_ptr->aud_pes_buf = buffer_create_max(1024 * 1024, 8 * 1024 * 1024);
    ctx_ptr->vid_pes_buf = buffer_create_max(1024 * 1024, 32 * 1024 * 1024);
    if(NULL == ctx_ptr->aud_pes_buf || NULL == ctx_ptr->vid_pes_buf) {
        close_ctx(ctx_ptr);
        return -2;
    }

    if ((flv_tag_flag & FLV_FLAG_AUDIO )== FLV_FLAG_AUDIO)
    {
        if (0 !=
            convert_AACDecoderConfiguration_to_adts(paras->aac_config,
            paras->aac_config_size,
            ctx_ptr->aud_seq_hdr,
            ctx_ptr->aud_seq_hdr_size,
            &ctx_ptr->aud_used_size,
            &ctx_ptr->aud_sample_rate)) 
        {
            close_ctx(ctx_ptr);
            return -3;
        }
    }

    if ( (flv_tag_flag & FLV_FLAG_VIDEO )== FLV_FLAG_VIDEO)
    {
        if (0 !=
            convert_AVCDecoderConfiguration_to_annexb(paras->avc_config,
            paras->avc_config_size,
            ctx_ptr->vid_seq_hdr,
            ctx_ptr->vid_seq_hdr_size,
            &ctx_ptr->vid_used_size))
        {
            close_ctx(ctx_ptr);
            return -4;
        }
    } 

    TS_INIT_PARA ts_init_para;

    ts_init_para.aud_stream_id = AUDIO_STREAM_ID;
    ts_init_para.vid_stream_id = VIDEO_STREAM_ID;

    ts_init_para.pcr_pid = ts_init_para.vid_pkt_pid = VIDEO_PACKET_PID;
    ts_init_para.aud_pkt_pid = AUDIO_PACKET_PID;
    ts_init_para.pmt_pid = PMT_PACKET_PID;

    ts_init_para.transportId = 0x0001;
    ts_init_para.bfirstInit = 0x0001;
    
    int video_flag = ((flv_tag_flag & FLV_FLAG_VIDEO) == FLV_FLAG_VIDEO);
    int audio_flag = ((flv_tag_flag & FLV_FLAG_AUDIO) == FLV_FLAG_AUDIO);

    if (0 != mpegts_init(&ctx_ptr->tshandle_ptr, &ts_init_para, video_flag, audio_flag)) {
        close_ctx(ctx_ptr);
        return -5;
    }

    mpegts_reset(ctx_ptr->tshandle_ptr);
    mpegts_write_pat(ctx_ptr->tshandle_ptr);
    mpegts_write_pmt(ctx_ptr->tshandle_ptr);
    int aud_frame_cnt = 0;
    int ts_packet_cnt = 2;
    int apes_cnt = 0, vpes_cnt = 0;
    int64_t pts = 0, dts = 0, cts = 0;
    uint8_t aud_stream_id = (uint8_t) (ts_init_para.aud_stream_id);
    uint8_t vid_stream_id = (uint8_t) (ts_init_para.vid_stream_id);
    buffer *pes_buf = NULL;
    flv_tag *tag = NULL;
    uint8_t *frame_ptr = NULL;
    size_t sizeof_frame = 0;
    uint8_t *pos = paras->flvdata;
    int is_first = 1;
    int64_t first_pts = -1;
    int64_t last_pts = -1;

    //int64_t audio_pts, video_pts, last_flv_ts;

    while(pos < paras->flvdata + paras->flvdata_size) {
        buffer_reset(ctx_ptr->vid_pes_buf);
        tag = (flv_tag *) pos;
        if(FLV_TAG_AUDIO != tag->type && FLV_TAG_VIDEO != tag->type) {
            pos += 4 + flv_get_datasize(tag->datasize) + sizeof(flv_tag);
            continue;
        }

        if((FLV_TAG_AUDIO == tag->type && flv_is_aac0(tag->data))
           || (FLV_TAG_VIDEO == tag->type && flv_is_avc0(tag->data))) {
            pos += 4 + flv_get_datasize(tag->datasize) + sizeof(flv_tag);
            continue;
        }

        pts = dts = flv_get_timestamp(tag->timestamp);
        //last_flv_ts = pts;
        if(dts < 0) {
            pts = dts = 0;
        }
        if(FLV_TAG_AUDIO == tag->type) {
            pes_buf = ctx_ptr->aud_pes_buf;
            frame_ptr = tag->data + sizeof(flv_tag_audio) + 1;
            sizeof_frame =
                flv_get_datasize(tag->datasize) - sizeof(flv_tag_audio) - 1;
            if(aud_frame_cnt % AUDIO_FRAMES_PES == 0) {
                buffer_reset(ctx_ptr->aud_pes_buf);
                if(0 != buffer_expand_capacity(pes_buf, 7)) {
                    close_ctx(ctx_ptr);
                    return -6;
                }
                //0
                buffer_append_ptr(pes_buf, "\x00\x00\x01", 3);
                //3
                buffer_append_ptr(pes_buf, &aud_stream_id, 1);
                //4
                buffer_append_ptr(pes_buf, "\x00\x00\x80", 3);

                if(0 != buffer_expand_capacity(pes_buf, 7)) {
                    close_ctx(ctx_ptr);
                    return -7;
                }
                //7
                buffer_append_ptr(pes_buf, "\x80\x05", 2);
                //9
                //if(!ctx_ptr->aud_sample_rate) {
                //    pts *= 90;
                //}
                //else { // this is god logic... I think it's pts*90.0 + 512000.0/sample_rate
                //    pts =
                //        (int64_t) ((1.0 * pts * ctx_ptr->aud_sample_rate +
                //                    512000) / (1024 * 1000));
                //    pts =
                //        (int64_t) ((1024 * 90 * 1000.0) * pts /
                //                   ctx_ptr->aud_sample_rate);
                //}

                pts *= 90;

                if(-1 == first_pts)
                    first_pts = pts / 90;
                last_pts = pts / 90;
                //audio_pts = last_pts;
                buffer_append_byte(pes_buf, 0x21 | ((pts >> 29) & 0x0e));
                //10
                buffer_append_byte(pes_buf, (pts >> 22) & 0xff);
                //11
                buffer_append_byte(pes_buf, 0x01 | ((pts >> 14) & 0xfe));
                //12
                buffer_append_byte(pes_buf, (pts >> 7) & 0xff);
                //13
                buffer_append_byte(pes_buf, 0x01 | ((pts & 0x7f) << 1));
            }

            if((0xff == *frame_ptr) && (0xf1 == *(frame_ptr + 1))) {
                if(0 != buffer_expand_capacity(pes_buf, sizeof_frame)) {
                    close_ctx(ctx_ptr);
                    return -8;
                }
                buffer_append_ptr(pes_buf, frame_ptr, sizeof_frame);
            }
            else {
                //
                size_t aac_frame_size = ADTS_HEADER_SIZE + sizeof_frame;

                //////////////////////////////////////////////////
                ctx_ptr->aud_seq_hdr[3] &= 0xfc;
                ctx_ptr->aud_seq_hdr[4] = 0x00;
                ctx_ptr->aud_seq_hdr[5] &= 0x1f;

                ctx_ptr->aud_seq_hdr[3] |= (aac_frame_size & 0x1fff) >> 11;
                ctx_ptr->aud_seq_hdr[4] |= (aac_frame_size & 0x7ff) >> 3;
                ctx_ptr->aud_seq_hdr[5] |= (aac_frame_size & 0x07) << 5;

                if(0 !=
                   buffer_expand_capacity(pes_buf,
                                          ctx_ptr->aud_used_size +
                                          sizeof_frame)) {
                    return -9;
                }
                buffer_append_ptr(pes_buf, ctx_ptr->aud_seq_hdr,
                                  ctx_ptr->aud_used_size);
                buffer_append_ptr(pes_buf, frame_ptr, sizeof_frame);
            }
            size_t aud_pes_len = buffer_data_len(pes_buf) - 6;

            if(aud_pes_len <= 0x00ffff) {
                uint8_t *pes_ptr = (uint8_t *) buffer_data_ptr(pes_buf);

                *(uint16_t *) (pes_ptr + 4) =
                    ((aud_pes_len & 0xff) << 8) | ((aud_pes_len >> 8) & 0xff);
            }
            if((aud_frame_cnt % AUDIO_FRAMES_PES) == (AUDIO_FRAMES_PES - 1)) {
                ts_packet_cnt += mpegts_write_pes(ctx_ptr->tshandle_ptr,
                                                  (char *)
                                                  buffer_data_ptr(pes_buf),
                                                  buffer_data_len(pes_buf),
                                                  aud_stream_id, 0);
                apes_cnt++;
            }

            aud_frame_cnt++;
        }
        if(FLV_TAG_VIDEO == tag->type) {
            pes_buf = ctx_ptr->vid_pes_buf;
            frame_ptr = tag->data + sizeof(flv_tag_video);
            sizeof_frame =
                flv_get_datasize(tag->datasize) - sizeof(flv_tag_video);
            flv_tag_video *tv = (flv_tag_video *) tag->data;
            size_t buf_need_sz =
                sizeof_frame + 9 + 6 + 10 + ctx_ptr->vid_used_size;
            if(buffer_capacity(pes_buf) < buf_need_sz) {
                if(0 !=
                   buffer_expand_capacity(pes_buf,
                                          buf_need_sz + (64 * 1024 -
                                                         (buf_need_sz &
                                                          0xffff)))) {
                    close_ctx(ctx_ptr);
                    return -10;
                }
            }
            int vid_seq_hdr_added = is_vid_seq_hdr_added(tag);
            int is_keyframe = FLV_KEYFRAME == tv->video_info.frametype;

            //0
            buffer_append_ptr(pes_buf, "\x00\x00\x01", 3);
            //3
            buffer_append_ptr(pes_buf, &vid_stream_id, 1);
            //4
            buffer_append_ptr(pes_buf, "\x00\x00\x80\xc0\x0a", 5);

            cts = FLV_UI24(tv->cts);
            if(cts < 0x800000)
                pts = dts + cts;
            pts *= 90;
            dts *= 90;
            if(-1 == first_pts)
                first_pts = pts / 90;
            last_pts = pts / 90;
            //video_pts = last_pts;
            //9
            buffer_append_byte(pes_buf, 0x31 | ((pts >> 29) & 0x0e));
            buffer_append_byte(pes_buf, (pts >> 22) & 0xff);
            buffer_append_byte(pes_buf, 0x01 | ((pts >> 14) & 0xfe));
            buffer_append_byte(pes_buf, (pts >> 7) & 0xff);
            buffer_append_byte(pes_buf, 0x01 | ((pts & 0x7f) << 1));

            // dts
            // 14
            buffer_append_byte(pes_buf, 0x11 | ((dts >> 29) & 0x0e));
            buffer_append_byte(pes_buf, (dts >> 22) & 0xff);
            buffer_append_byte(pes_buf, 0x01 | ((dts >> 14) & 0xfe));
            buffer_append_byte(pes_buf, (dts >> 7) & 0xff);
            buffer_append_byte(pes_buf, 0x01 | ((dts & 0x7f) << 1));

            //19
            buffer_append_ptr(pes_buf, "\x00\x00\x00\x01\x09\xe0", 6);
            //25
            size_t vid_pes_len = sizeof_frame + 3 + 6 + 10;

            if((vid_seq_hdr_added && is_keyframe) || is_first) {
                buffer_append_ptr(pes_buf, ctx_ptr->vid_seq_hdr,
                                  ctx_ptr->vid_used_size);
                vid_pes_len += ctx_ptr->vid_used_size;
                is_first = 0;
            }
            if(vid_pes_len <= 0x00ffff) {
                uint8_t *pes_ptr = (uint8_t *) buffer_data_ptr(pes_buf);

                *(uint16_t *) (pes_ptr + 4) =
                    ((vid_pes_len & 0xff) << 8) | ((vid_pes_len >> 8) & 0xff);
            }
            uint8_t *buf_frame_ptr =
                (uint8_t *) buffer_data_ptr(pes_buf) +
                buffer_data_len(pes_buf);
            buffer_append_ptr(pes_buf, frame_ptr, sizeof_frame);
            deal_vid_frame(buf_frame_ptr, sizeof_frame);
            ts_packet_cnt += mpegts_write_pes_with_pcr(ctx_ptr->tshandle_ptr,
                                              (char *)
                                              buffer_data_ptr(pes_buf),
                                              vid_pes_len + 6, vid_stream_id,
                                              is_keyframe,
                                              pts,
                                              0);
            vpes_cnt++;
        }
        pos += 4 + flv_get_datasize(tag->datasize) + sizeof(flv_tag);
    }

    if(0x00 != (aud_frame_cnt % AUDIO_FRAMES_PES)) {
        ts_packet_cnt += mpegts_write_pes(ctx_ptr->tshandle_ptr,
                                          (char *) buffer_data_ptr(ctx_ptr->
                                                                   aud_pes_buf),
                                          buffer_data_len(ctx_ptr->
                                                          aud_pes_buf),
                                          aud_stream_id, 0);
        apes_cnt++;
    }
    mpegts_write_dummy(ctx_ptr->tshandle_ptr);
    unsigned char *ts_obuf = NULL;
    int sizeofbuf = mpegts_get_buffer(ctx_ptr->tshandle_ptr, &ts_obuf);

    if(0 != buffer_expand_capacity(obuf, sizeofbuf)) {
        return -20;
    }

    // printf("s1: %d, e1: %d, a1: %d, v1: %d t1: %d ", first_pts, last_pts, audio_pts, video_pts, last_flv_ts);

    *duration = last_pts - first_pts;
    buffer_append_ptr(obuf, ts_obuf, sizeofbuf);
    close_ctx(ctx_ptr);
    return 0;
}
