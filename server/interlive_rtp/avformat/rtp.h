#pragma once

#include <stdint.h>
#ifndef WIN32
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#else
#include <Windows.h>
#endif

namespace avformat
{

    enum RTPTransport
    {
        RTP_AVP = 1,
        RTP_AVPF = 2,
    };

    enum RTPAVType
    {
        RTP_AV_NULL = 0,
        RTP_AV_MP3 = 14,
        RTP_AV_H264 = 96,
        RTP_AV_AAC = 97,
		RTP_AV_AAC_GXH = 115,
		RTP_AV_AAC_MAIN = 116,
        RTP_AV_FEC = 127,
		RTP_AV_F_FEC = 126,
        RTP_AV_ALL = 255,
    };

    enum RTPMediaType
    {
        RTP_MEDIA_NULL = 0,
        RTP_MEDIA_AUDIO = 1,
        RTP_MEDIA_VIDEO = 2,
        RTP_MEDIA_BOTH = 3,
    };
    /******************************************************************
    RTP_FIXED_HEADER
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |V=2|P|X|  CC   |M|     PT      |       sequence number         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                           timestamp                           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |           synchronization source (SSRC) identifier            |
    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
    |            contributing source (CSRC) identifiers             |
    |                             ....                              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    ******************************************************************/
    typedef struct
    {
        /* byte 0 */
        uint8_t csrc_len : 4; /* CC expect 0 */
        uint8_t extension : 1;/* X  expect 1, see RTP_OP below */
        uint8_t padding : 1;  /* P  expect 0 */
        uint8_t version : 2;  /* V  expect 2 */
        /* byte 1 */
        uint8_t payload : 7; /* PT  RTP_PAYLOAD_RTSP */
        uint8_t marker : 1;  /* M   expect 1 */
        /* byte 2,3 */
        uint16_t seq_no;   /*sequence number*/
        /* byte 4-7 */
        uint32_t timestamp;
        /* byte 8-11 */
        uint32_t ssrc; /* stream number is used here. */
        uint8_t data[0];

        uint32_t get_ssrc() const
        {
            return ntohl(ssrc);
        }

        uint32_t set_ssrc(uint32_t input_ssrc)
        {
            ssrc = htonl(input_ssrc);
            return input_ssrc;
        }

        uint16_t get_seq() const
        {
            return ntohs(seq_no);
        }

        uint16_t set_seq(uint16_t seq)
        {
            seq_no = htons(seq);
            return seq;
        }

        uint32_t get_rtp_timestamp() const
        {
            return ntohl(timestamp);
        }

        uint32_t set_rtp_timestamp(uint32_t rtp_timestamp)
        {
            timestamp = htonl(rtp_timestamp);
            return rtp_timestamp;
        }

        uint32_t get_ms_timestamp(uint32_t sample_rate) const
        {
            uint32_t rtp_timestamp = ntohl(timestamp);
            return ((uint64_t)rtp_timestamp) * 1000 / sample_rate;
        }

        uint32_t set_ms_timestamp(uint32_t ms_timestamp, uint32_t sample_rate)
        {
            uint32_t rtp_timestamp = ((uint64_t)ms_timestamp)* sample_rate / 1000;
            timestamp = htonl(rtp_timestamp);
            return rtp_timestamp;
        }
    } RTP_FIXED_HEADER;/*12 bytes*/

    typedef struct {

        short rtp_extend_profile;

        short rtp_extend_length;


    } EXTEND_HEADER;

	typedef struct
	{
		/* byte 0 */
		uint8_t csrc_len : 4; /* CC expect 0 */
		uint8_t extension : 1;/* X  expect 1, see RTP_OP below */
		uint8_t padding : 1;  /* P  expect 0 */
		uint8_t MSK : 2;  /* V  expect 2 */
		/* byte 1 */
		uint8_t payload : 7; /* PT  RTP_PAYLOAD_RTSP */
		uint8_t marker : 1;  /* M   expect 1 */
		/* byte 2,3 */
		uint16_t length_recovery;
		/* byte 4-7 */
		uint32_t timestamp;
		/* byte 8 */
		uint8_t ssrc_count;
		/* byte 9-11*/
		uint8_t reserved[3]; /* reserved[2] as fec_d_l*/
		/* byte 12-15 */
		uint32_t ssrc; /* stream number is used here. */
		/* byte 16-17 */
		uint16_t sn_base;
		/* byte 18 */
		uint8_t M;
		/* byte 19 */
		uint8_t N;

		uint8_t data[0];

		uint8_t get_MSK() const
		{
			return MSK;
		}

		void set_MSK(uint8_t _msk)
		{
			MSK = _msk;
		}


		uint32_t get_ssrc() const
		{
			return ntohl(ssrc);
		}

		uint32_t set_ssrc(uint32_t input_ssrc)
		{
			ssrc = htonl(input_ssrc);
			return input_ssrc;
		}

		uint16_t get_sn_base() const {
			return ntohs(sn_base);
		}
		void set_sn_base(uint16_t sn) {
			sn_base = htons(sn);
		}

		uint16_t get_length_recovery() const {
			return length_recovery;
		}

		void set_length_recovery(uint16_t len) {
			length_recovery = len;
		}


		uint32_t get_timestamp() const {
			return ntohl(timestamp);
		}

		void set_timestamp(uint32_t t) {
			timestamp = htonl(t);
		}

		uint8_t get_ssrc_count() const {
			return ssrc_count;
		}

		void set_ssrc_count(uint8_t val) {
			ssrc_count = val;
		}

		uint8_t get_M() const {
			return M;
		}

		void set_M(uint8_t val) {
			M = val;
		}

		uint8_t get_N() const {
			return N;
		}

		void set_N(uint8_t val) {
			N = val;
		}

		uint8_t get_payload_type() const {
			return payload;
		}

		void set_payload_type(uint8_t p) {
			payload = p;
		}

	}F_FEC_HEADER;


    /************************************************************************/
    /* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |             SN base             |          length recovery    |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |E| PT recovery |                      mask                     |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         TS recovery                           |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                                                                     */
    /************************************************************************/



    typedef struct
    {
        uint16_t sn_base;
        uint16_t length_recovery;
        uint8_t  payload : 7;
        uint8_t  E : 1;
        uint8_t  mask[3];
        uint32_t timestamp;
        uint8_t data[0];

        uint16_t get_sn_base() const {
            return ntohs(sn_base);
        }
        void set_sn_base(uint16_t sn) {
            sn_base = htons(sn);
        }

        uint16_t get_length_recovery() const {
            return length_recovery;
        }

        void set_length_recovery(uint16_t len) {
            length_recovery = len;
        }

        bool get_extension() const {
            return E == 1;
        }

        void set_extension(bool e) {
            E = e ? 1 : 0;
        }

        uint8_t get_payload_type() const {
            return payload;
        }

        void set_payload_type(uint8_t p) {
            payload = p;
        }

        uint32_t get_mask() const {
            return mask[0] << 16 | mask[1] << 8 | mask[2];
        }

        void set_mask(uint32_t m) {
            mask[0] = m >> 16 & 0xff;
            mask[1] = m >> 8 & 0xff;
            mask[2] = m & 0xff;
        }

        uint32_t get_timestamp() const {
            return ntohl(timestamp);
        }

        void set_timestamp(uint32_t t) {
            timestamp = htonl(t);
        }

    } FEC_HEADER;

    typedef struct
    {
        uint8_t csrc_len : 4;  /* CC expect 0 */
        uint8_t extension : 1;  /* X  expect 1, see RTP_OP below */
        uint8_t padding : 1;  /* P  expect 0 */
        uint8_t marker : 1;  /* M  marker */
        uint8_t flags : 1;  /* reserved */
    } FEC_EXTEND_HEADER;

    /******************************************************************
    NALU_HEADER
    +---------------+
    |0|1|2|3|4|5|6|7|
    +-+-+-+-+-+-+-+-+
    |F|NRI|  Type   |
    +---------------+
    ******************************************************************/
    typedef struct {
        //byte 0  
        unsigned char TYPE : 5;
        unsigned char NRI : 2;
        unsigned char F : 1;
    } NALU_HEADER; /* 1 byte */


    /******************************************************************
    FU_INDICATOR
    +---------------+
    |0|1|2|3|4|5|6|7|
    +-+-+-+-+-+-+-+-+
    |F|NRI|  Type   |
    +---------------+
    ******************************************************************/
    typedef struct {
        //byte 0  
        unsigned char TYPE : 5;
        unsigned char NRI : 2;
        unsigned char F : 1;
    } FU_INDICATOR; /*1 byte */


    /******************************************************************
    FU_HEADER
    +---------------+
    |0|1|2|3|4|5|6|7|
    +-+-+-+-+-+-+-+-+
    |S|E|R|  Type   |
    +---------------+
    ******************************************************************/
    typedef struct {
        //byte 0  
        unsigned char TYPE : 5;
        unsigned char R : 1;
        unsigned char E : 1;
        unsigned char S : 1;
    } FU_HEADER; /* 1 byte */


    typedef struct
    {
        //    int startcodeprefix_len;      //! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)  
        uint32_t len;                 //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)  
        uint32_t max_size;            //! Nal Unit Buffer size  
        int forbidden_bit;            //! should be always FALSE  
        int nal_reference_idc;        //! NALU_PRIORITY_xxxx  
        int nal_unit_type;            //! NALU_TYPE_xxxx      
        uint8_t *buf;                    //! contains the first byte followed by the EBSP
        //    unsigned short lost_packets;  //! true, if packet loss is detected  
    } NALU_t;

    struct LATM_HEADER
    {
        char au_count[2];
        char first_au_size[2];
    };

}
