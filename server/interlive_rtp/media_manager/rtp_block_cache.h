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


#pragma once

#include <deque>
#include <vector>
#include "fragment/rtp_block.h"
#include "avformat/sdp.h"
#include "streamid.h"
#include "util/port.h"

namespace media_manager
{
    class MediaManagerRTPInterface;

    class RTPSessionItem
    {
    public:
        RTPSessionItem()
        {
            ssrc = 0;
            seq = 0;
        }

        uint32_t ssrc;
        uint16_t seq;
    };

    class RTPMediaItem
    {
    public:
        RTPMediaItem()
            :audio_item(items[0]),
            video_item(items[1])
        {

        }

        RTPSessionItem items[2];

        RTPSessionItem& audio_item;
        RTPSessionItem& video_item;

    };

    class DLLEXPORT RTPCircularCache
    {
    public:
        RTPCircularCache(StreamId_Ext& stream_id);
        virtual ~RTPCircularCache();

        int32_t set_manager(MediaManagerRTPInterface*);

        virtual uint32_t set_sample_rate(uint32_t sample_rate);
        virtual uint32_t set_max_duration_ms(uint32_t max_duration);
        virtual uint32_t set_max_size(uint32_t max_size);

        virtual int32_t set_rtp(const avformat::RTP_FIXED_HEADER*, uint16_t len, int32_t& status);
        virtual int32_t empty_count();

        virtual avformat::RTP_FIXED_HEADER* get_by_seq(uint16_t seq, uint16_t& len, int32_t& status_code, bool return_next_valid_packet = false);
        virtual avformat::RTP_FIXED_HEADER* get_latest(uint16_t& len, int32_t& status_code);
        virtual avformat::RTP_FIXED_HEADER* get_next_by_seq(uint16_t seq, uint16_t& len, int32_t& status_code);

        virtual uint16_t size();
        virtual uint16_t max_size();

        virtual bool  is_inited();

        uint32_t get_ssrc();
        void reset();
        void clean();

        void on_timer();

        time_t get_push_active();
        uint64_t get_last_push_relative_timestamp_ms();

    protected:
        void _set_push_active();
       
        uint32_t _adjust();

        int _fill_in(const avformat::RTP_FIXED_HEADER*, uint16_t);

        void _push_back(const avformat::RTP_FIXED_HEADER*, uint16_t);

        StreamId_Ext _stream_id;
        MediaManagerRTPInterface* _media_manager;

        uint32_t _max_duration;
        uint32_t _sample_rate;

        uint32_t _ssrc;
        avformat::RTPAVType _av_type;
        avformat::RTPMediaType _media_type;

        uint16_t _max_size;
        uint16_t _last_push_seq;

        uint32_t _last_push_tick_timestamp;
        uint64_t _last_push_relative_timestamp_ms;

        time_t _push_active;

        int32_t _push_timeout_seconds;

        std::deque<fragment::RTPBlock> _circular_cache;
    };

    class DLLEXPORT RTPMediaCache
    {
    public:
        RTPMediaCache(StreamId_Ext& stream_id);
        virtual ~RTPMediaCache();

        virtual int32_t set_manager(MediaManagerRTPInterface*);

        virtual int32_t set_sdp(std::string& sdp);
        virtual int32_t set_sdp(const char* sdp, int32_t len);
        //        int32_t set_sdp(avformat::SdpInfo* sdp, bool malloc_new = false);

        virtual avformat::SdpInfo* get_sdp();

        virtual avformat::SdpInfo* get_sdp(int32_t& status);

        virtual int32_t set_rtp(const avformat::RTP_FIXED_HEADER* rtp, uint16_t len, int32_t& status);

        int32_t set_uploader_ntp(avformat::RTPAVType type, uint32_t ntp_secs, uint32_t ntp_frac, uint32_t rtp);
        int32_t get_uploader_ntp(avformat::RTPAVType &type, uint32_t &ntp_secs, uint32_t &ntp_frac, uint32_t &rtp);

 //       avformat::RTP_FIXED_HEADER* get_by_media_item(RTPMediaItem& item, uint16_t& len, int32_t& status_code, bool return_next_valid_packet = false);

        virtual avformat::RTP_FIXED_HEADER* get_latest_by_media_item(RTPMediaItem& item, uint16_t& len, int32_t& status_code);
        virtual avformat::RTP_FIXED_HEADER* get_next_by_media_item(RTPMediaItem& item, uint16_t& len, int32_t& status_code);


        virtual RTPCircularCache* get_cache_by_ssrc(uint32_t ssrc);
        virtual RTPCircularCache* get_cache_by_media(avformat::RTPMediaType);
        virtual RTPCircularCache* get_cache_by_codec(avformat::RTPAVType);

        virtual RTPCircularCache* get_audio_cache();
        virtual RTPCircularCache* get_video_cache();

        virtual time_t get_push_active_time();
        virtual time_t set_push_active_time();

        virtual uint64_t get_last_push_relative_timestamp_ms();

        virtual void adjust_cache_size();

        virtual bool rtp_empty();

        virtual void on_timer();

    protected:
        int32_t _reset_cache();


    public:
        StreamId_Ext _stream_id;
        avformat::SdpInfo* _sdp;

        RTPCircularCache* _audio_cache;
        RTPCircularCache* _video_cache;

        int32_t _push_timeout_seconds;

        time_t _push_active;

        MediaManagerRTPInterface* _media_manager;

        uint64_t _last_push_relative_timestamp_ms;

        avformat::RTPAVType _ntp_type;
        uint32_t _ntp_secs;
        uint32_t _ntp_frac;
        uint32_t _ntp_rtp;
    };
}
