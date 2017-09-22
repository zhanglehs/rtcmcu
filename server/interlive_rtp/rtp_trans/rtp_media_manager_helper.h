/*
 * Author: hechao@youku.com 
 *
 */
#pragma once

#include "avformat/sdp.h"
#include "media_manager/media_manager_rtp_interface.h"
#include "streamid.h"

class RTPMMHelper
{
    public:
        virtual ~RTPMMHelper(){};

        virtual int32_t set_rtp(const StreamId_Ext& stream_id, const avformat::RTP_FIXED_HEADER *rtp, uint16_t len, int32_t& status_code) = 0;
        virtual const avformat::RTP_FIXED_HEADER* get_rtp_by_ssrc_seq(const StreamId_Ext& stream_id, uint32_t ssrc, 
            uint16_t seq, uint16_t &len, int32_t& status_code, bool return_next_valid_packet = false) = 0;

        virtual const avformat::RTP_FIXED_HEADER* get_rtp_by_mediatype_seq(const StreamId_Ext& stream_id, avformat::RTPMediaType type, 
            uint16_t seq, uint16_t &len, int32_t& status_code, bool return_next_valid_packet = false) = 0;

        virtual int32_t set_sdp_char(const StreamId_Ext& stream_id, const char* sdp, int32_t len, int32_t& status_code) = 0;
        virtual const char* get_sdp_char(const StreamId_Ext& stream_id, int32_t& len, int32_t& status_code) = 0;

        virtual int32_t set_sdp_str(const StreamId_Ext& stream_id, const std::string& sdp, int32_t& status_code) = 0;
        virtual std::string get_sdp_str(const StreamId_Ext& stream_id, int32_t& status_code) = 0;

        virtual int32_t set_uploader_ntp(const StreamId_Ext& stream_id, avformat::RTPAVType type, uint32_t ntp_secs, uint32_t ntp_frac, uint32_t rtp) = 0;
};

class RTPMediaManagerHelper: public RTPMMHelper
{
    public:
        RTPMediaManagerHelper();
        ~RTPMediaManagerHelper();

    public:
        virtual int32_t set_rtp(const StreamId_Ext& stream_id, const avformat::RTP_FIXED_HEADER *rtp, uint16_t len, int32_t& status_code);
        virtual const avformat::RTP_FIXED_HEADER* get_rtp_by_ssrc_seq(const StreamId_Ext& stream_id, uint32_t ssrc,
            uint16_t seq, uint16_t &len, int32_t& status_code, bool return_next_valid_packet = false);

        virtual const avformat::RTP_FIXED_HEADER* get_rtp_by_mediatype_seq(const StreamId_Ext& stream_id, avformat::RTPMediaType type,
            uint16_t seq, uint16_t &len, int32_t& status_code, bool return_next_valid_packet);

        virtual int32_t set_sdp_char(const StreamId_Ext& stream_id, const char* sdp, int32_t len, int32_t& status_code);
        virtual const char* get_sdp_char(const StreamId_Ext& stream_id, int32_t& len, int32_t& status_code);

        virtual int32_t set_sdp_str(const StreamId_Ext& stream_id, const std::string& sdp, int32_t& status_code);
        virtual std::string get_sdp_str(const StreamId_Ext& stream_id, int32_t& status_code);

        virtual int32_t set_uploader_ntp(const StreamId_Ext& stream_id, avformat::RTPAVType type, uint32_t ntp_secs, uint32_t ntp_frac, uint32_t rtp);

    protected:
        media_manager::MediaManagerRTPInterface* _media_manager;
};
