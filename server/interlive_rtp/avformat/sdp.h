#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include "rtp.h"
#include <sstream>
#include "../util/port.h"

namespace avformat
{
    class sdp_session_level
    {
    public:
        sdp_session_level& operator=(const sdp_session_level& right);

        int sdp_version;      /**< protocol version (currently 0) */
        int id;               /**< session ID */
        int version;          /**< session version */
        int start_time;       /**< session start time (NTP time, in seconds),
                              or 0 in case of permanent session */
        int end_time;         /**< session end time (NTP time, in seconds),
                              or 0 if the session is not bounded */
        int ttl;              /**< TTL, in case of multicast stream */
        std::string user;     /**< username of the session's creator */
        std::string src_addr; /**< IP address of the machine from which the session was created */
        std::string src_type; /**< address type of src_addr */
        std::string dst_addr; /**< destination IP address (can be multicast) */
        std::string dst_type; /**< destination IP address type */
        std::string name;     /**< session name (can be an empty string) */
    };

    class rtp_media_info
    {
    public:
        rtp_media_info();
        ~rtp_media_info();
        rtp_media_info(const rtp_media_info& info);

        //        rtp_media_info& operator=(const rtp_media_info& right);

        uint32_t channels;
        uint32_t rate;
        uint8_t *extra_data;
        uint32_t extra_data_len;
        std::string dest_addr;
		uint16_t constant_duration;
        uint16_t payload_type;
        uint16_t dest_port;
        uint32_t packetization_mode;
        uint32_t h264_profile_level_id;
        uint32_t rtp_transport;
    };

    typedef struct SDPParseState {
        /* SDP only */
        int            default_ttl;
        int            skip_media;  ///< set if an unknown m= line occurs
        int            seen_rtpmap;
        int            seen_fmtp;
        char           delayed_fmtp[2048];
    } SDPParseState;

#define SPACE_CHARS " \t\r\n"

    class DLLEXPORT SdpInfo
    {
    public:
        SdpInfo();
        ~SdpInfo();
        const std::vector<rtp_media_info*>& get_media_infos();
        sdp_session_level* get_sdp_header();

        void parse_sdp_str(const char *sdp_str, uint32_t len);
        std::string generate_sdp_str();
        int set_dest_audio_addr(char *host, uint16_t port);
        int set_dest_video_addr(char *host, uint16_t port);
        std::string load(const char*, int);
        std::string get_sdp_str();
        const char* get_sdp_char();
        std::string get_sdp_str() const;
        const char* get_sdp_char() const;
        int32_t length();

        int add_media(rtp_media_info *s);
        int clear_media();

    private:
        void sdp_write_address(std::stringstream& ss, const char *dest_addr, const char *dest_type, int ttl);
        void sdp_write_header(std::stringstream& ss, struct sdp_session_level *s);
        void sdp_write_media(std::stringstream& ss, struct rtp_media_info *s);
        void sdp_write_media_attributes(std::stringstream& ss, struct rtp_media_info *s);
        char *extradata2psets(struct rtp_media_info *s, char* psets);
        char *extradata2config(struct rtp_media_info *s, char* config);
        uint8_t *avc_find_startcode(const uint8_t *p, const uint8_t *end);
        uint8_t *avc_find_startcode_internal(const uint8_t *p, const uint8_t *end);
        static char *base64_encode(char *out, int out_size, const uint8_t *in, int in_size);
        static int base64_decode(uint8_t *out, const char *in_str, int out_size);
        static char *data_to_hex(char *buff, const uint8_t *src, int s, int lowercase);
        static int hex_to_data(uint8_t *data, const char *p);
        static int strstart(const char *str, const char *pfx, const char **ptr);
        static void get_word(char *buf, int buf_size, const char **pp);
        static void get_word_until_chars(char *buf, int buf_size,
            const char *sep, const char **pp);
        static void get_word_sep(char *buf, int buf_size, const char *sep,
            const char **pp);
        static int isspace(int c);
        void parse_sdp_line(SDPParseState *s1, int letter, const char *buf);
        static int parse_h264_sdp_line(rtp_media_info *s, const char *attr, const char *value);
        static int parse_aac_sdp_line(rtp_media_info *s, const char *attr, const char *value);
        static int rtp_next_attr_and_value(const char **p, char *attr, int attr_size,
            char *value, int value_size);
        static int parse_fmtp(rtp_media_info *s, const char *line,
            int(*parse_fmtp_func)(rtp_media_info *s, const char *attr, const char *value));
        static int h264_parse_sprop_parameter_sets(rtp_media_info *s, uint8_t *data_ptr, uint32_t& size_ptr, const char *value);

    private:
        sdp_session_level* _sdp_header;
        std::vector<rtp_media_info*> _media_infos;
        std::stringstream _sdp_ss;
        char* _sdp_char;

    };
}
