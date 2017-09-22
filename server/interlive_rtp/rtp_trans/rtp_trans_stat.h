/*
 * author: hechao@youku.com
 * create: 2015/11/17
 */

#ifndef __RTP_TRANS_STAT_H_
#define __RTP_TRANS_STAT_H_

struct RTPTransStat
{
    RTPTransStat()  {}
    RTPTransStat(const RTPTransStat &right)
    {
        video_frac_lost_packet      = right.video_frac_lost_packet;
        audio_frac_lost_packet      = right.audio_frac_lost_packet;
        video_total_lost_packet     = right.video_total_lost_packet;
        audio_total_lost_packet     = right.audio_total_lost_packet;
        video_total_rtp_packet      = right.video_total_rtp_packet;
        audio_total_rtp_packet      = right.audio_total_rtp_packet;
        video_rtt_ms                = right.video_rtt_ms;
        audio_rtt_ms                = right.audio_rtt_ms;
        video_total_bitrate         = right.video_total_bitrate;
        audio_total_bitrate         = right.audio_total_bitrate;
        video_effect_bitrate        = right.video_effect_bitrate;
        audio_effect_bitrate        = right.audio_effect_bitrate;
        video_total_bytes           = right.video_total_bytes;
        audio_total_bytes           = right.audio_total_bytes;
        video_packet_lost_rate      = right.video_packet_lost_rate;
        audio_packet_lost_rate      = right.audio_packet_lost_rate;
    }

    RTPTransStat & operator=(const RTPTransStat &right)
    {
        video_frac_lost_packet      = right.video_frac_lost_packet;
        audio_frac_lost_packet      = right.audio_frac_lost_packet;
        video_total_lost_packet     = right.video_total_lost_packet;
        audio_total_lost_packet     = right.audio_total_lost_packet;
        video_total_rtp_packet      = right.video_total_rtp_packet;
        audio_total_rtp_packet      = right.audio_total_rtp_packet;
        video_rtt_ms                = right.video_rtt_ms;
        audio_rtt_ms                = right.audio_rtt_ms;
        video_total_bitrate         = right.video_total_bitrate;
        audio_total_bitrate         = right.audio_total_bitrate;
        video_effect_bitrate        = right.video_effect_bitrate;
        audio_effect_bitrate        = right.audio_effect_bitrate;
        video_total_bytes           = right.video_total_bytes;
        audio_total_bytes           = right.audio_total_bytes;
        video_packet_lost_rate      = right.video_packet_lost_rate;
        audio_packet_lost_rate      = right.audio_packet_lost_rate;
        return *this;
    }

    uint32_t video_frac_lost_packet;
    uint32_t audio_frac_lost_packet;
    uint32_t video_total_lost_packet;
    uint32_t audio_total_lost_packet;
    uint32_t video_total_rtp_packet;
    uint32_t audio_total_rtp_packet;
    uint32_t video_rtt_ms;
    uint32_t audio_rtt_ms;
    uint32_t video_total_bitrate;
    uint32_t audio_total_bitrate;
    uint32_t video_effect_bitrate;
    uint32_t audio_effect_bitrate;
    uint64_t video_total_bytes;
    uint64_t audio_total_bytes;
    float    video_packet_lost_rate;
    float    audio_packet_lost_rate;
};

#endif

