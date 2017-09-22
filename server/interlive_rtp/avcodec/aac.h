#pragma once

#include <stdint.h>
#include "../../util/bits.h"

#define MAX_EXTRADATA_SIZE 7 /* LC + SBR + PS config */
#define AACPLUS_AOT_AAC_LC 2
#define AACPLUS_AOT_SBR 5
#define AACPLUS_LOAS_SYNC 0x2b7
#define AACPLUS_PS_EXT 0x548

#define ADTS_HEADER_SIZE 7

namespace avcodec
{
    struct adts_fixed_header
    {
        uint16_t syncword : 12;
        uint8_t	ID : 1;
        uint8_t	layer : 2;
        uint8_t	protection_absent : 1;
        uint8_t	profile : 2;
        uint8_t sampling_frequency_index : 4;
        uint8_t	private_bit : 1;
        uint8_t channel_configuration : 3;
        uint8_t original_copy : 1;
        uint8_t	home : 1;
    };
    struct adts_variable_header
    {
        uint8_t  copyright_identification_bit : 1;
        uint8_t	 copyright_identification_start : 1;
        uint16_t frame_length : 13;
        uint16_t adts_buffer_fullness : 11;
        uint8_t	 number_of_raw_data_blocks_in_frame : 2;
    };

    uint8_t aac_get_samplingFrequencyIndex(uint32_t samplerate);

    uint32_t aac_get_samplingFrequency_by_index(uint8_t i);

    int aac_get_channel_by_index(int ch);

    class AACConfig
    {
    private:
        //normal 
        bool    _is_sbr;
        uint8_t _audio_object;
        uint32_t _sample_freq;
        uint8_t _channel_config;
        uint32_t _frame_length;
        uint8_t _audioSpecificConfig[MAX_EXTRADATA_SIZE];

        int downSampledSBR;
        int forceUpSampling;
    private:
        int8_t _GASpecificConfig(bitfile *ld);
    public:
        AACConfig();

        void set_audioObjectType(uint8_t audioObjectType);
        void set_samplingFrequency(uint32_t samplingFrequency);
        void set_channelConfig(uint8_t channels);
        void set_frame_length(uint32_t length);
        void set_sbr(bool is_sbr);

        uint8_t get_audioObjectType();
        uint32_t get_samplingFrequency();
        uint8_t get_channelConfig();
        uint32_t get_frame_length();
        bool is_sbr();

        int parse_aac_specific(uint8_t* data, int len);
        int parse_adts_header(uint8_t* data, int len);
        uint8_t * build_aac_specific(int& len);
        uint8_t * build_aac_adts(int& len);
    };
}
