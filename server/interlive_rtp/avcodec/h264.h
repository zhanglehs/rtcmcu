#pragma once

#include <stdint.h>

namespace avcodec
{
    enum nal_unit_type_e
    {
        NAL_UNKNOWN = 0,
        NAL_SLICE = 1,
        NAL_SLICE_DPA = 2,
        NAL_SLICE_DPB = 3,
        NAL_SLICE_DPC = 4,
        NAL_SLICE_IDR = 5,    /* ref_idc != 0 */
        NAL_SEI = 6,    /* ref_idc == 0 */
        NAL_SPS = 7,
        NAL_PPS = 8,
        NAL_AUD = 9,
        NAL_FILLER = 12,
        /* ref_idc == 0 for 6,9,10,11,12 */
    };

    class BitVector {
    public:
        BitVector(unsigned char* baseBytePtr,
            unsigned baseBitOffset,
            unsigned totNumBits);

        void setup(unsigned char* baseBytePtr,
            unsigned baseBitOffset,
            unsigned totNumBits);

        void putBits(unsigned from, unsigned numBits); // "numBits" <= 32
        void put1Bit(unsigned bit);

        unsigned getBits(unsigned numBits); // "numBits" <= 32
        unsigned get1Bit();
        bool get1BitBoolean() { return get1Bit() != 0; }

        void skipBits(unsigned numBits);

        unsigned curBitIndex() const { return fCurBitIndex; }
        unsigned totNumBits() const { return fTotNumBits; }
        unsigned numBitsRemaining() const { return fTotNumBits - fCurBitIndex; }

        unsigned get_expGolomb();
        // Returns the value of the next bits, assuming that they were encoded using an exponential-Golomb code of order 0

    private:
        unsigned char* fBaseBytePtr;
        unsigned fBaseBitOffset;
        unsigned fTotNumBits;
        unsigned fCurBitIndex;
    };

    // A general bit copy operation:
    void shiftBits(unsigned char* toBasePtr, unsigned toBitOffset,
        unsigned char const* fromBasePtr, unsigned fromBitOffset,
        unsigned numBits);

    typedef struct
    {
        int i_ref_idc;  /* nal_priority_e */
        int i_type;     /* nal_unit_type_e */
        int b_long_startcode;
        int i_first_mb; /* If this NAL is a slice, the index of the first MB in the slice. */
        int i_last_mb;  /* If this NAL is a slice, the index of the last MB in the slice. */

        /* Size of payload (including any padding) in bytes. */
        int     i_payload;
        /* If param->b_annexb is set, Annex-B bytestream with startcode.
        * Otherwise, startcode is replaced with a 4-byte size.
        * This size is the size used in mp4/similar muxing; it is equal to i_payload-4 */
        uint8_t *p_payload;

        /* Size of padding in bytes. */
        int i_padding;
    } x264_nal_t;

    uint8_t* create_h264_payload(x264_nal_t* x264_nal_list, uint32_t nal_num, uint32_t& payload_len);

    const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end);

    class CH264ConfigParser{
    private:
        // Fields in H.264 headers, used in parsing:
        unsigned log2_max_frame_num; // log2_max_frame_num_minus4 + 4
        bool separate_colour_plane_flag;
        bool frame_mbs_only_flag;

        int m_width;
        int m_height;
        unsigned num_units_in_tick;
        unsigned time_scale;
        unsigned fixed_frame_rate_flag;

        void analyze_vui_parameters(BitVector& bv);

    public:
        CH264ConfigParser();
        ~CH264ConfigParser();

        bool ParseSPS(const unsigned char *, int buflen);

        int getWidth(){ return m_width; };
        int getHeight(){ return m_height; };
        int getTimeScale(){ return time_scale; };
        int getNumUnits(){ return num_units_in_tick; };

        static int AVCEncoderConfigurationRecord(uint8_t* buffer, int maxLen,
            uint8_t* sps, int sps_size, uint8_t* pps, int pps_size);
        static int AVCDecoderConfigurationRecord(uint8_t* buffer, int maxLen,
            uint8_t* sps, int& sps_size, uint8_t* pps, int& pps_size);
    };
}
