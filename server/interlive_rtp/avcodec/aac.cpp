#include "aac.h"
#include <stdio.h>
#include "string.h"

namespace avcodec
{
    static unsigned const samplingFrequencyTable[16] = 
    {
        96000, 88200, 64000, 48000,
        44100, 32000, 24000, 22050,
        16000, 12000, 11025, 8000,
        7350, 0, 0, 0
    };

    const uint8_t mpeg4audio_channels[8] = {
        0, 1, 2, 3, 4, 5, 6, 8
    };

    static uint8_t ObjectTypesTable[32] = {
        0, /*  0 NULL */
        1, /*  1 AAC Main */
        1, /*  2 AAC LC */
        0, /*  3 AAC SSR */
        0, /*  4 AAC LTP */
        1, /*  5 SBR */
        0, /*  6 AAC Scalable */
        0, /*  7 TwinVQ */
        0, /*  8 CELP */
        0, /*  9 HVXC */
        0, /* 10 Reserved */
        0, /* 11 Reserved */
        0, /* 12 TTSI */
        0, /* 13 Main synthetic */
        0, /* 14 Wavetable synthesis */
        0, /* 15 General MIDI */
        0, /* 16 Algorithmic Synthesis and Audio FX */

        /* MPEG-4 Version 2 */
        0, /* 17 ER AAC LC */
        0, /* 18 (Reserved) */
        0, /* 19 ER AAC LTP */
        0, /* 20 ER AAC scalable */
        0, /* 21 ER TwinVQ */
        0, /* 22 ER BSAC */
        0, /* 23 ER AAC LD */
        0, /* 24 ER CELP */
        0, /* 25 ER HVXC */
        0, /* 26 ER HILN */
        0, /* 27 ER Parametric */
        0, /* 28 (Reserved) */
        0, /* 29 (Reserved) */
        0, /* 30 (Reserved) */
        0  /* 31 (Reserved) */
    };

    uint8_t aac_get_samplingFrequencyIndex(uint32_t samplerate)
    {
        for (uint8_t i = 0; i < sizeof(samplingFrequencyTable); i++)
        {
            if (samplerate >= samplingFrequencyTable[i])
            {
                return i;
            }
        }
        return 4;
    }

    uint32_t aac_get_samplingFrequency_by_index(uint8_t i) {
        return samplingFrequencyTable[i];
    }

    int aac_get_channel_by_index(int ch)
    {
        int i;
        for (i = 0; i < 16; i++) {
            if (ch == mpeg4audio_channels[i])
                return i;
        }
        return 16 - 1;
    }

    AACConfig::AACConfig() {
        _audio_object = 0;;
        _sample_freq = 0;
        _channel_config = 0;
        _frame_length = 0;
        _is_sbr = false;
        memset(_audioSpecificConfig, 0, sizeof(_audioSpecificConfig));
    }

    void AACConfig::set_audioObjectType(uint8_t audioObjectType) {
        _audio_object = audioObjectType;
    }
    void AACConfig::set_samplingFrequency(uint32_t samplingFrequencyIndex) {
        _sample_freq = samplingFrequencyIndex;
    }
    void AACConfig::set_channelConfig(uint8_t channels) {
        _channel_config = channels;
    }

    void AACConfig::set_frame_length(uint32_t length) {
        _frame_length = length;
    }

    uint8_t AACConfig::get_audioObjectType() {
        return _audio_object;
    }
    uint32_t AACConfig::get_samplingFrequency() {
        return _sample_freq;
    }
    uint8_t AACConfig::get_channelConfig() {
        return _channel_config;
    }

    uint32_t AACConfig::get_frame_length() {
        return _frame_length;
    }

    bool AACConfig::is_sbr() {
        return _is_sbr;
    }

    int8_t AACConfig::_GASpecificConfig(bitfile *ld)
    {
        //program_config pce;

        /* 1024 or 960 */
        bitbuffer_get1bit(ld);  // frameLengthFlag

        int dependsOnCoreCoder = bitbuffer_get1bit(ld);
        if (dependsOnCoreCoder == 1)
        {
            bitbuffer_getbits(ld, 14);  // coreCoderDelay
        }

        bitbuffer_get1bit(ld);  // extensionFlag
        if (_channel_config == 0)
        {
            //if (program_config_element(&pce, ld))
            //    return -3;
            //assert(0);
        }

        return 0;
    }


    int AACConfig::parse_aac_specific(uint8_t* data, int len) {


        bitfile ld;
        bitbuffer_initbits(&ld, data, len);

        int8_t result = 0;
        uint32_t startpos = bitbuffer_get_processed_bits(&ld);
        int8_t bits_to_decode = 0;

        _audio_object = (uint8_t)bitbuffer_getbits(&ld, 5);

        int samplingFrequencyIndex = (uint8_t)bitbuffer_getbits(&ld, 4);
        if (samplingFrequencyIndex == 0x0f)
            bitbuffer_getbits(&ld, 24);

        _channel_config = (uint8_t)bitbuffer_getbits(&ld, 4);

        _sample_freq = aac_get_samplingFrequency_by_index(samplingFrequencyIndex);

        if (ObjectTypesTable[_audio_object] != 1)
        {
            return -1;
        }

        if (_sample_freq == 0)
        {
            return -2;
        }

        if (_channel_config > 7)
        {
            return -3;
        }

        /* check if we have a mono file */
        if (_channel_config == 1)
        {
            /* upMatrix to 2 channels for implicit signalling of PS */
            _channel_config = 2;
        }

        _is_sbr = false;
        if (_audio_object == 5)
        {
            uint8_t tmp;

            _is_sbr = true;
            tmp = (uint8_t)bitbuffer_getbits(&ld, 4);
            /* check for downsampled SBR */
            if (tmp == samplingFrequencyIndex)
                downSampledSBR = 1;
            samplingFrequencyIndex = tmp;
            if (samplingFrequencyIndex == 15)
            {
                _sample_freq = (uint32_t)bitbuffer_getbits(&ld, 24);
            }
            else {
                _sample_freq = aac_get_samplingFrequency_by_index(samplingFrequencyIndex);
            }
            _audio_object = (uint8_t)bitbuffer_getbits(&ld, 5);
        }

        /* get GASpecificConfig */
        if (_audio_object == 1 || _audio_object == 2 ||
            _audio_object == 3 || _audio_object == 4 ||
            _audio_object == 6 || _audio_object == 7)
        {
            result = _GASpecificConfig(&ld);
        }
        else {
            result = -4;
        }

        bits_to_decode = (int8_t)(len * 8 - (startpos - bitbuffer_get_processed_bits(&ld)));

        if ((_audio_object != 5) && (bits_to_decode >= 16))
        {
            int16_t syncExtensionType = (int16_t)bitbuffer_getbits(&ld, 11);

            if (syncExtensionType == 0x2b7)
            {
                uint8_t tmp_OTi = (uint8_t)bitbuffer_getbits(&ld, 5);

                if (tmp_OTi == 5)
                {
                    _is_sbr = (uint8_t)bitbuffer_get1bit(&ld);

                    if (_is_sbr)
                    {
                        uint8_t tmp;

                        /* Don't set OT to SBR until checked that it is actually there */
                        _audio_object = tmp_OTi;

                        tmp = (uint8_t)bitbuffer_getbits(&ld, 4);

                        /* check for downsampled SBR */
                        if (tmp == samplingFrequencyIndex)
                            downSampledSBR = 1;
                        samplingFrequencyIndex = tmp;

                        if (samplingFrequencyIndex == 15)
                        {
                            _sample_freq = (uint32_t)bitbuffer_getbits(&ld, 24);
                        }
                        else {
                            _sample_freq = aac_get_samplingFrequency_by_index(samplingFrequencyIndex);
                        }
                    }
                }
            }
        }

        /* no SBR signalled, this could mean either implicit signalling or no SBR in this file */
        /* MPEG specification states: assume SBR on files with samplerate <= 24000 Hz */
        if (!_is_sbr)
        {
            if (_sample_freq <= 24000)
            {
                _sample_freq *= 2;
                forceUpSampling = 1;
            }
            else /* > 24000*/ {
                downSampledSBR = 1;
            }
        }

        bitbuffer_endbits(&ld);

        return result;
    }

    int AACConfig::parse_adts_header(uint8_t* data, int len) {
        bitfile ld;
        bitbuffer_initbits(&ld, data, len);
        //syncword : 12
        bitbuffer_getbits(&ld, 12);
        //ID : 1
        bitbuffer_getbits(&ld, 1);
        //layer : 2
        bitbuffer_getbits(&ld, 2);
        //protection_absent : 1
        bitbuffer_getbits(&ld, 1);
        //profile : 2
        _audio_object = (uint8_t)bitbuffer_getbits(&ld, 2);
        //sampling_frequency_index : 4
        _sample_freq = aac_get_samplingFrequency_by_index(bitbuffer_getbits(&ld, 4));
        //private_bit : 1
        bitbuffer_getbits(&ld, 1);
        //channel_configuration : 3
        _channel_config = (uint8_t)bitbuffer_getbits(&ld, 3);
        //original_copy : 1
        bitbuffer_getbits(&ld, 1);
        //home : 1
        bitbuffer_getbits(&ld, 1);
        //copyright_identification_bit : 1
        bitbuffer_getbits(&ld, 1);
        //copyright_identification_start : 1
        bitbuffer_getbits(&ld, 1);
        //frame_length : 13
        _frame_length = bitbuffer_getbits(&ld, 13);
        //adts_buffer_fullness : 11
        bitbuffer_getbits(&ld, 11);
        //number_of_raw_data_blocks_in_frame : 2
        bitbuffer_getbits(&ld, 2);

        if (_sample_freq <= 24000)
        {
            _sample_freq = _sample_freq * 2;
            if (_channel_config == 1)
            {
              // TODO: zhangle, this maybe a mistake
                _channel_config = 2;
            }
            _is_sbr = true;
        }
        return 0;
    }

    uint8_t * AACConfig::build_aac_specific(int& len) {

        
        if (_is_sbr)
        {
            int srate_idx, ch_idx;
            int window_size = 0;
            int loas_sync = AACPLUS_LOAS_SYNC;
            int ps_extension = 0;

            ps_extension = AACPLUS_PS_EXT;

            srate_idx = aac_get_samplingFrequencyIndex(_sample_freq/2);
            ch_idx = aac_get_channel_by_index(_channel_config == 2 ? 1:_channel_config);
            //if (nSamplesPerFrame != 1024)
            //    window_size = 1;
            _audioSpecificConfig[0] = AACPLUS_AOT_AAC_LC << 3 | srate_idx >> 1;
            _audioSpecificConfig[1] = srate_idx << 7 | ch_idx << 3 | window_size << 2;

            srate_idx = aac_get_samplingFrequencyIndex(_sample_freq);

            _audioSpecificConfig[2] = loas_sync >> 3; //sync extension
            _audioSpecificConfig[3] = ((loas_sync << 5) & 0xe0) | AACPLUS_AOT_SBR; //sync extension + sbr hdr
            _audioSpecificConfig[4] = 1 << 7 | srate_idx << 3 | ps_extension >> 8;
            if (ps_extension) {
                _audioSpecificConfig[5] = ps_extension & 0xff;
                _audioSpecificConfig[6] = 1 << 7;
                len = 7;
            }
            else {
                len = 5;
            }
        }
        else {
            int srate_idx, ch_idx;
            srate_idx = aac_get_samplingFrequencyIndex(_sample_freq);
            ch_idx = aac_get_channel_by_index(_channel_config);

            _audioSpecificConfig[0] = (uint8_t)(0x10 | ((_audio_object > 2) ? 2 : _audio_object << 3) | ((srate_idx >> 1) & 0x07));
            _audioSpecificConfig[1] = (uint8_t)(((srate_idx & 0x01) << 7) | ((ch_idx & 0x0F) << 3));
            len = 2;
        }
        return _audioSpecificConfig;
    }

    uint8_t * AACConfig::build_aac_adts(int& len) {
        
        int sampling_frequency_index = avcodec::aac_get_samplingFrequencyIndex(_is_sbr?_sample_freq/2:_sample_freq); //3:48k 4:44.1k
        int channel_configuration = _is_sbr ? 1 : _channel_config;
        int syncword = 0xfff;
        int id = 0x0;
        int layer = 0x00;
        int protection_absent = 0x1;
        //4C 8*
        int profile_objecttype = 0x01;
        int private_bit = 0;
        int original_copy = 0;
        int home = 0;


        //*0 2b 9f fc
        int copyright_identification_bit = 0; //  1b bslbf
        int copyright_identification_start = 0;// 1b bslbf
        int frame_length = _frame_length;
        int adts_buffer_fullness = 0x7FF; //11 b 
        int number_of_raw_data_blocks_in_frame = 0;//2b 
        int i = 0;
        _audioSpecificConfig[i++] = (uint8_t)((syncword >> 4) & 0xFF);//0
        _audioSpecificConfig[i++] =
            (uint8_t)((syncword & 0x0F) << 4 | ((id & 0x01) << 3)
            | (layer & 0x03) << 1 | (protection_absent & 0x01));//1
        /**
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |     buf[2]    |     buf[3]    |     buf[4]    |     buf[5]    |     buf[6]    |
        * |0                  |1                  |2                  |3                  |
        * |0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9|
        * | T |SampleR|P|  C  |O|H|B|S|     aac_frame_length    |        fullness     |Num|
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        */
        _audioSpecificConfig[i++] = (uint8_t)((profile_objecttype & 0x03) << 6
            | (sampling_frequency_index & 0x0F) << 2
            | (private_bit & 0x01) << 1
            | (channel_configuration & 0x07) >> 2);//2
        _audioSpecificConfig[i++] = (uint8_t)((channel_configuration & 0x07) << 6
            | (original_copy & 0x01) << 5
            | (home & 0x01) << 4
            | (copyright_identification_bit & 0x01) << 3
            | (copyright_identification_start & 0x01) << 2
            | (frame_length & 0x01FF) >> 11);//3
        _audioSpecificConfig[i++] = (uint8_t)((frame_length >> 3) & 0xFF);//4
        _audioSpecificConfig[i++] = (uint8_t)(((frame_length & 0x07) << 5)
            | ((adts_buffer_fullness >> 6) & 0x1F));//5
        _audioSpecificConfig[i++] = (uint8_t)((adts_buffer_fullness & 0x3F) << 2
            | (number_of_raw_data_blocks_in_frame & 0x07));//6

        len = ADTS_HEADER_SIZE;
        return _audioSpecificConfig;
    }
}
