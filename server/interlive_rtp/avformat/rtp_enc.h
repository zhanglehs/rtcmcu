#pragma once

#include <stdint.h>
#include "avcodec/avcodec.h"
#include <vector>
#include "rtp.h"
#include "../util/port.h"

namespace avformat
{
    enum OutputFormat
    {
        AAC_LATM,
        AAC_ADTS
    };
    class DLLEXPORT RTPData
    {
    public:
        RTP_FIXED_HEADER* header;
        uint16_t len;
    };

    class DLLEXPORT RTPEncode
    {
    public:
        RTPEncode();
        int32_t init(uint32_t ssrc, uint32_t sample_rate,
            uint16_t max_rtp_packet_len,
            uint16_t init_rtp_packet_count);
        virtual ~RTPEncode();

        virtual std::vector<RTPData>& generate_packet(
            const uint8_t* payload, uint32_t payload_len,
            uint32_t timestamp, int32_t& status) = 0;

    protected:
        virtual uint16_t get_max_payload_len();
        
        std::vector<RTP_FIXED_HEADER*> _temp_packets;
        uint32_t _next_temp_packet;
        std::vector<RTPData> _rtp_data_vec;

        uint32_t _seq_num;
        uint32_t _ssrc;

        bool _initialized;

        uint16_t _max_rtp_packet_len;
        uint16_t _init_rtp_packet_count;
        uint32_t _sample_rate;
    };

    class DLLEXPORT RTPEncodeH264 : public RTPEncode
    {
    public:
        virtual std::vector<RTPData>& generate_packet(
            const uint8_t* payload, uint32_t payload_len,
            uint32_t timestamp, int32_t& status);
        virtual ~RTPEncodeH264();

    protected:
        void nal_generate(const uint8_t *buf, uint32_t timestamp, int size, int last);
        RTP_FIXED_HEADER* get_current_packet();
        RTP_FIXED_HEADER* build_rtp_header(RTP_FIXED_HEADER*, uint32_t timestamp, int maker);
//        virtual uint16_t get_max_payload_len();
    };

    class DLLEXPORT RTPEncodeAAC : public RTPEncode
    {
    public:
        RTPEncodeAAC(OutputFormat out_format = AAC_ADTS) : _out_format(out_format), _channel(2) {
        }
        virtual std::vector<RTPData>& generate_packet(
            const uint8_t* payload, uint32_t payload_len,
            uint32_t timestamp, int32_t& status);
        void set_channel(int channel);
        virtual ~RTPEncodeAAC();

    protected:
        RTP_FIXED_HEADER* build_rtp_header(RTP_FIXED_HEADER*, uint32_t timestamp, int maker);
    private:
        OutputFormat _out_format;
        int      _channel;
    };

}
