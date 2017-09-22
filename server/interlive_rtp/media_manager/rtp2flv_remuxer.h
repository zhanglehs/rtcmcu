#ifndef _FLV2RTP_REMUXER_H_
#define _FLV2RTP_REMUXER_H_

#include "cache_manager.h"
#include "../avformat/rtp.h"
#include "../avformat/sdp.h"
#include "../avformat/FLV.h"
#include "rtp_block_cache.h"
#include "jitter_buffer.h"
#include "../rtp_trans/rtp_media_manager_helper.h"
#include "../whitelist_manager.h"
#include <appframe/array_object_pool.hpp>

namespace media_manager {
    class RTP2FLVRemuxer : public RTPMediaManagerHelper, public WhitelistObserver {
        class StreamMeta {
        public:
            FLV *_flv;
            FLVHeader *_flv_header;
            media_manager::JitterBuffer *_audio_buffer;
            media_manager::JitterBuffer *_video_buffer;
            buffer *_video_frame_buffer;
            std::queue<int32_t> _video_frame_buffer_pos;
            uint32_t _video_frame_ts;
            uint32_t _muxer_queue_threshold;
            bool     _use_nalu_split_code;

            uint32_t _last_ts;
            uint16_t _last_video_seq;
            uint32_t _cur_nalu_size;

            StreamId_Ext _stream_id;

            uint32_t _audio_time_base;
            uint32_t _audio_channel;
            uint32_t _video_time_base;

            //rtp timestamp
            uint32_t _last_rtp_video_ts;
            uint32_t _last_rtp_audio_ts;

            //flv timestamp
            uint32_t _first_video_ts;
            uint32_t _first_audio_ts;
            uint32_t _video_offset;
            uint32_t _audio_offset;
            uint32_t _video_inc_req;
            uint32_t _audio_inc_req;
            uint32_t _out_sync_ts;

            bool  is_cur_frame_valied;
             

            bool is_sps_reach;
            bool is_pps_reach;
            bool is_adts_reach;
            bool is_audio_only;
            bool is_check_frame;
            buffer *_avc0_buffer;
            buffer *_aac0_buffer;
            RTPMediaCache *_media_cache;

            std::deque<flv_tag *> _audio_out_queue;
            std::deque<flv_tag *> _video_out_queue;
            

        public:
            media_manager::RTPMediaCache *get_media_cache();
            StreamMeta(StreamId_Ext stream_id);
            ~StreamMeta();
        };

    public:
        static RTP2FLVRemuxer * get_instance() {
            if (_inst == NULL)
            {
                _inst = new RTP2FLVRemuxer;
            }
            return _inst;
        }
        ~RTP2FLVRemuxer();
        virtual int32_t set_rtp(const StreamId_Ext& stream_id, const avformat::RTP_FIXED_HEADER *rtp, uint16_t len, int32_t& status_code);
        const avformat::RTP_FIXED_HEADER* get_rtp_by_ssrc_seq(const StreamId_Ext& stream_id, uint32_t ssrc,
            uint16_t seq, uint16_t &len, int32_t& status_code, bool return_next_valid_packet = false);
        virtual const avformat::RTP_FIXED_HEADER* get_rtp_by_mediatype_seq(const StreamId_Ext& stream_id, avformat::RTPMediaType type,
            uint16_t seq, uint16_t &len, int32_t& status_code, bool return_next_valid_packet);

        virtual int32_t set_sdp_char(const StreamId_Ext& stream_id, const char* sdp, int32_t len, int32_t& status_code);
        virtual const char* get_sdp_char(const StreamId_Ext& stream_id, int32_t& len, int32_t& status_code);

        virtual int32_t set_sdp_str(const StreamId_Ext& stream_id, const std::string& sdp, int32_t& status_code);
        virtual std::string get_sdp_str(const StreamId_Ext& stream_id, int32_t& status_code);

        void update(const StreamId_Ext &streamid, WhitelistEvent ev);
    protected:
        RTP2FLVRemuxer();
        int32_t _set_rtp_to_buffer(StreamMeta *stream_meta, const avformat::RTP_FIXED_HEADER* rtp, uint16_t len, int32_t& status);

        int32_t _set_flv_to_mm(StreamMeta *stream_meta);

        void _write_header(StreamMeta *stream_meta);
        void _put_video_frame(StreamMeta *stream_meta);
        StreamMeta * _get_stream_meta_by_sid(const StreamId_Ext& stream_id);
    private:
        typedef __gnu_cxx::hash_map<StreamId_Ext, StreamMeta *> META_MAP;
        META_MAP _meta_map;
        UploaderCacheManagerInterface* _uploader_cache_instance;
	static RTP2FLVRemuxer *_inst;
    };
}
#endif
