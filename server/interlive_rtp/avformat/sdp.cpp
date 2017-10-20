/**
* @file sdp.cpp
* @brief class SdpInfo
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2015/05/20
* @see sdp_info.cpp
*
*/

#include "../util/log.h"
#include "sdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sstream>

using namespace std;

namespace avformat
{
    sdp_session_level& sdp_session_level::operator = (const sdp_session_level& right)
    {
        this->sdp_version = right.sdp_version;
        this->id = right.id;
        this->version = right.version;
        this->start_time = right.start_time;
        this->end_time = right.end_time;
        this->ttl = right.ttl;
        this->user = right.user;
        this->src_addr = right.src_addr;
        this->src_type = right.src_type;
        this->dst_addr = right.dst_addr;
        this->dst_type = right.dst_type;
        this->name = right.name;

        return *this;
    }

    rtp_media_info::rtp_media_info()
        :channels(0),
        rate(0),
        extra_data(NULL),
        extra_data_len(0),
        constant_duration(1024),
        payload_type(0),
        dest_port(0),
        packetization_mode(0),
        h264_profile_level_id(0),
        rtp_transport(0)
    {
        extra_data = new uint8_t[2048];
        memset(extra_data, 0, 2048);
        extra_data_len = 0;
    }

    rtp_media_info::~rtp_media_info()
    {
        if (extra_data != NULL)
        {
            delete[] extra_data;
            extra_data = NULL;
        }
    }

    rtp_media_info::rtp_media_info(const rtp_media_info& right)
    {
        this->channels = right.channels;
        this->rate = right.rate;
        this->extra_data_len = right.extra_data_len;
        this->dest_addr = right.dest_addr;
        this->payload_type = right.payload_type;
        this->dest_port = right.dest_port;
        this->packetization_mode = right.packetization_mode;
        this->h264_profile_level_id = right.h264_profile_level_id;
        this->rtp_transport = right.rtp_transport;
		this->constant_duration = right.constant_duration;

        if (right.extra_data == NULL || right.extra_data_len == 0)
        {
            return;
        }
        else
        {
            memcpy(this->extra_data, right.extra_data, this->extra_data_len);
        }
    }

    SdpInfo::SdpInfo() {
        _sdp_header = new sdp_session_level();
        _sdp_char = new char[4096];
        memset(_sdp_char, 0, 4096);
    }

    string SdpInfo::load(const char* sdp, int len)
    {
        if (sdp == NULL || len <= 0)
        {
            ERR("sdp is invalid");
            return string("");
        }

        _sdp_ss.clear();
        _sdp_ss.str("");
        _sdp_ss << sdp;

        parse_sdp_str(sdp, len);

        return _sdp_ss.str();
    }

    string SdpInfo::get_sdp_str()
    {
        if (_sdp_char[0] == '\0' && _media_infos.size() > 0)
            generate_sdp_str();

        return _sdp_ss.str();
    }

    const char* SdpInfo::get_sdp_char()
    {
        if (_sdp_char[0] == '\0' && _media_infos.size() > 0)
            generate_sdp_str();

        return _sdp_char;
    }

    string SdpInfo::get_sdp_str() const
    {
        return _sdp_ss.str();
    }

    const char* SdpInfo::get_sdp_char() const
    {
        return _sdp_char;
    }

    int32_t SdpInfo::length()
    {
        return _sdp_ss.str().length();
    }

    int SdpInfo::add_media(rtp_media_info *s)
    {
        _media_infos.push_back(s);
        return _media_infos.size();
    }

    int SdpInfo::clear_media()
    {
        while (_media_infos.size() > 0)
        {
            delete _media_infos.back();
            _media_infos.pop_back();
        }

        return 0;
    }

    SdpInfo::~SdpInfo()
    {
        clear_media();

        if (_sdp_header != NULL)
        {
            delete _sdp_header;
            _sdp_header = NULL;
        }

        if (_sdp_char != NULL)
        {
            delete[] _sdp_char;
            _sdp_char = NULL;
        }
    }

    sdp_session_level* SdpInfo::get_sdp_header()
    {
        return _sdp_header;
    }

    const vector<rtp_media_info*>& SdpInfo::get_media_infos()
    {
        return _media_infos;
    }

    void SdpInfo::parse_sdp_str(const char *sdp_str, uint32_t len)
    {
        const char *p;
        int letter;
        char buf[16384], *q;
        SDPParseState s1;

        p = sdp_str;
        for (;;) {
            p += strspn(p, SPACE_CHARS);
            letter = *p;
            if (letter == '\0')
                break;
            p++;
            if (*p != '=')
                goto next_line;
            p++;
            /* get the content */
            q = buf;
            while (*p != '\n' && *p != '\r' && *p != '\0' && (p - sdp_str) <= len)
            {
                if ((q - buf) < (uint32_t)sizeof(buf)-1)
                    *q++ = *p;
                p++;
            }
            *q = '\0';
            //sdp_parse_line(s, s1, letter, buf);
            parse_sdp_line(&s1, letter, buf);
        next_line:
            while (*p != '\n' && *p != '\0')
                p++;
            if (*p == '\n')
                p++;
        }
    }



    void SdpInfo::parse_sdp_line(SDPParseState *s1, int letter, const char *buf)
    {
        char buf1[64], st_type[64];
        const char *p;
        //int payload_type;
        rtp_media_info *md;
        p = buf;
        rtp_media_info* media_info;

        switch (letter) {
        case 'v': // version
            get_word(buf1, sizeof(buf1), &p);
            _sdp_header->sdp_version = atoi(buf1);
            break;
        case 'o': // origin
            get_word(buf1, sizeof(buf1), &p);
            _sdp_header->user = buf1;
            get_word(buf1, sizeof(buf1), &p);
            _sdp_header->id = atoi(buf1);
            get_word(buf1, sizeof(buf1), &p);
            _sdp_header->version = atoi(buf1);
            get_word(buf1, sizeof(buf1), &p);
            get_word(buf1, sizeof(buf1), &p);
            _sdp_header->src_type = buf1;
            get_word(buf1, sizeof(buf1), &p);
            _sdp_header->src_addr = buf1;
            break;
        case 's': // name
            get_word_sep(buf1, sizeof(buf1), "\r", &p);
            _sdp_header->name = buf1;
            break;
        case 't':
            get_word(buf1, sizeof(buf1), &p);
            _sdp_header->start_time = atoi(buf1);
            get_word(buf1, sizeof(buf1), &p);
            _sdp_header->end_time = atoi(buf1);
            break;
        case 'c':
            get_word(buf1, sizeof(buf1), &p);
            if (strcmp(buf1, "IN") != 0)
                return;
            get_word(buf1, sizeof(buf1), &p);
            if (_media_infos.size() == 0)
            {
                _sdp_header->dst_type = buf1;
            }

            if (strcmp(buf1, "IP4"))
                return;
            get_word_sep(buf1, sizeof(buf1), "/", &p);
            if (_media_infos.size() == 0)
            {
                _sdp_header->dst_addr = buf1;
            }
            else
            {
                rtp_media_info* info = _media_infos.back();
                info->dest_addr = buf1;
            }
            break;
        case 'm':
            /* new stream */
            s1->skip_media = 0;
            s1->seen_fmtp = 0;
            s1->seen_rtpmap = 0;

            media_info = new rtp_media_info();

            get_word(st_type, sizeof(st_type), &p);
            get_word(buf1, sizeof(buf1), &p); /* port */
            media_info->dest_port = atoi(buf1);
            get_word(buf1, sizeof(buf1), &p); /* protocol */
            get_word(buf1, sizeof(buf1), &p); /* format list */
            media_info->payload_type = atoi(buf1);

            _media_infos.push_back(media_info);

            break;
        case 'a':
            if (strstart(p, "rtpmap:", &p) && _media_infos.size() > 0) {
                /* NOTE: rtpmap is only supported AFTER the 'm=' tag */
                get_word(buf1, sizeof(buf1), &p);
                //payload_type = atoi(buf1);

                if (_media_infos.size() > 0) {
                    md = _media_infos[_media_infos.size() - 1];
                    //sdp_parse_rtpmap(s, md, payload_type, p);
                    get_word_sep(buf1, sizeof(buf1), "/ ", &p);
                    if (strcasecmp(buf1, "H264") == 0)
                    {
                        md->payload_type = RTP_AV_H264;
                    }
                    else if (strcasecmp(buf1, "mpeg4-generic") == 0)
                    {
                        md->payload_type = RTP_AV_AAC;
                    }
                    get_word_sep(buf1, sizeof(buf1), "/", &p);
                    md->rate = atoi(buf1);
                    if (RTP_AV_AAC == md->payload_type)
                    {
                        get_word_sep(buf1, sizeof(buf1), "/", &p);
                        md->channels = atoi(buf1);
                    }
                }
                s1->seen_rtpmap = 1;
                if (s1->seen_fmtp)
                {
                    md = _media_infos[_media_infos.size() - 1];
                    if (RTP_AV_H264 == md->payload_type)
                    {
                        parse_fmtp(md, s1->delayed_fmtp, this->parse_h264_sdp_line);
                    }
                    else if (RTP_AV_AAC == md->payload_type)
                    {
                        parse_fmtp(md, s1->delayed_fmtp, this->parse_aac_sdp_line);
                    }

                }
            }
            else if (strstart(p, "fmtp:", &p)) {
                // let dynamic protocol handlers have a stab at the line.
                get_word(buf1, sizeof(buf1), &p);
                //payload_type = atoi(buf1);
                if (s1->seen_rtpmap && _media_infos.size() > 0)
                {
                    md = _media_infos.back();
                    //parse_fmtp(s, rt, payload_type, buf);
                    if (RTP_AV_H264 == md->payload_type)
                    {
                        parse_fmtp(md, p, parse_h264_sdp_line);
                    }
                    else if (RTP_AV_AAC == md->payload_type)
                    {
                        parse_fmtp(md, p, parse_aac_sdp_line);
                    }
                }
                else {
                    s1->seen_fmtp = 1;
                    memcpy(s1->delayed_fmtp, buf, sizeof(s1->delayed_fmtp));
                }
            }
            break;
        }
    }

    string SdpInfo::generate_sdp_str()
    {
        //int len = 0;
        _sdp_ss.clear();
        _sdp_ss.str("");
        sdp_write_header(_sdp_ss, _sdp_header);
        for (unsigned int i = 0; i < _media_infos.size(); i++)
        {
            sdp_write_media(_sdp_ss, _media_infos[i]);
        }

        string s = _sdp_ss.str();

        memcpy(_sdp_char, s.c_str(), s.length());
        _sdp_char[s.length()] = 0;

        return s;
    }

    int SdpInfo::set_dest_audio_addr(char *host, uint16_t port)
    {
        return 0;
    }

    int SdpInfo::set_dest_video_addr(char *host, uint16_t port)
    {
        return 0;
    }

    void SdpInfo::sdp_write_address(stringstream& ss, const char *dest_addr, const char *dest_type, int ttl)
    {
        if (dest_addr && (strlen(dest_addr) > 0))
        {
            if (!dest_type)
                dest_type = "IP4";
            char buff[1024];

            if (ttl > 0 && !strcmp(dest_type, "IP4"))
            {
                /* The TTL should only be specified for IPv4 multicast addresses,
                * not for IPv6. */
                sprintf(buff, "c=IN %s %s/%d\r\n", dest_type, dest_addr, ttl);
            }
            else
            {
                sprintf(buff, "c=IN %s %s\r\n", dest_type, dest_addr);
            }
            ss << buff;
        }
    }

    void SdpInfo::sdp_write_header(stringstream& ss, struct sdp_session_level *s)
    {
        char buff[1024];
        sprintf(buff, "v=%d\r\n"
            "o=- %d %d IN %s %s\r\n"
            "s=%s\r\n",
            s->sdp_version,
            s->id, s->version, s->src_type.c_str(), s->src_addr.c_str(),
            s->name.c_str());
        ss << buff;

        sdp_write_address(ss, s->dst_addr.c_str(), s->dst_type.c_str(), s->ttl);

        sprintf(buff, "t=%d %d\r\na=tool:YouKu Media Server\r\n", 0, 0);
        ss << buff;
    }

    void SdpInfo::sdp_write_media(stringstream& ss, struct rtp_media_info *s)
    {

        const char *type;
        const char *transport;
        int payload_type = s->payload_type;

        switch (s->payload_type) {
        case RTP_AV_H264: type = "video"; break;
        case RTP_AV_AAC: type = "audio"; break;
        case RTP_AV_MP3: type = "audio"; break;
        default: type = "application"; break;
        }

        switch (s->rtp_transport)
        {
        case RTP_AVP:
            transport = "RTP/AVP";
            break;

        case RTP_AVPF:
            transport = "RTP/AVPF";
            break;

        default:
            transport = "RTP/AVP";
            break;
        }

        char buff[1024];
        sprintf(buff, "m=%s %d %s %d\r\n", type, s->dest_port, transport, payload_type);
        ss << buff;

        sdp_write_address(ss, s->dest_addr.c_str(), "IP4", _sdp_header->ttl);

        //if (s->rate) {
        //	sprintf_s(buff, size, "b=AS:%d\r\n", s->rate);
        //}

        sdp_write_media_attributes(ss, s);
    }

    void SdpInfo::sdp_write_media_attributes(std::stringstream& ss, struct rtp_media_info *s)
    {
        char config[1024];
        char buff[1024];
        config[0] = 0;

        switch (s->payload_type)
        {
        case RTP_AV_H264:
            if (s->extra_data_len)
            {
                extradata2psets(s, config);
            }
            else
            {
                sprintf(buff, "a=rtpmap:%d H264/90000\r\n",
                    s->payload_type);
                ss << buff;
                return;
            }

            if (strlen(config) == 0)
            {
                return;
            }
            sprintf(buff, "a=rtpmap:%d H264/90000\r\na=fmtp:%d packetization-mode=%d%s\r\n",
                s->payload_type, s->payload_type, 1, config);
            ss << buff;
            break;

        case RTP_AV_AAC:
            if (s->extra_data_len)
            {
                extradata2config(s, config);
            }
            else
            {
                WRN("AAC with no global headers is currently not supported.\n");
                return;
            }

            if (strlen(config) == 0)
            {
                return;
            }
            sprintf(buff, "a=rtpmap:%d MPEG4-GENERIC/%d/%d\r\n",
                s->payload_type, s->rate, s->channels);
            ss << buff;
            sprintf(buff, "a=fmtp:%d profile-level-id=1;mode=AAC-hbr;constantDuration=%d;sizelength=13;indexlength=3;indexdeltalength=3%s\r\n",
                s->payload_type,s->constant_duration, config);
            ss << buff;
            break;

        case RTP_AV_MP3:

            break;
        default:
            break;
        }
    }

    char *SdpInfo::extradata2psets(struct rtp_media_info *s, char* psets)
    {
        char *p;
        const uint8_t *r;
        static const char pset_string[] = "; sprop-parameter-sets=";
        static const char profile_string[] = "; profile-level-id=";
        uint8_t *extradata = s->extra_data;
        int extradata_size = s->extra_data_len;
        const uint8_t *sps = NULL, *sps_end;

        if (s->extra_data_len > 1024)
        {
            WRN("Too much extradata! payload_type %d extra_len %d addr %s \n", s->payload_type, s->extra_data_len, s->dest_addr.c_str());
            return NULL;
        }
        /*if (c->extradata[0] == 1) {
        if (ff_avc_write_annexb_extradata(c->extradata, &extradata,
        &extradata_size))
        return NULL;
        tmpbuf = extradata;
        }*/

        memcpy(psets, pset_string, strlen(pset_string));
        p = psets + strlen(pset_string);
        r = avc_find_startcode(extradata, extradata + extradata_size);
        while (r < extradata + extradata_size)
        {
            const uint8_t *r1;
            uint8_t nal_type;

            while (!*(r++));
            nal_type = *r & 0x1f;
            r1 = avc_find_startcode(r, extradata + extradata_size);
            if (nal_type != 7 && nal_type != 8) { /* Only output SPS and PPS */
                r = r1;
                continue;
            }
            if (p != (psets + strlen(pset_string)))
            {
                *p = ',';
                p++;
            }
            if (!sps) {
                sps = r;
                sps_end = r1;
            }
            if (!base64_encode(p, 1024 - (p - psets), r, r1 - r))
            {
                WRN("Cannot Base64-encode %d %d!\n", (int)(1024 - (p - psets)), (int)(r1 - r));
                return NULL;
            }
            p += strlen(p);
            r = r1;
        }
        if (sps && sps_end - sps >= 4)
        {
            memcpy(p, profile_string, strlen(profile_string));
            p += strlen(profile_string);
            data_to_hex(p, sps + 1, 3, 0);
            p[6] = '\0';
        }

        return psets;
    }

    char *SdpInfo::extradata2config(struct rtp_media_info *s, char* config)
    {
        if (s->extra_data_len> 1024)
        {
            WRN("Too large extradata!\n");
            return NULL;
        }

        memcpy(config, "; config=", 9);
        data_to_hex(config + 9, s->extra_data, s->extra_data_len, 0);
        config[9 + s->extra_data_len * 2] = 0;

        return config;
    }

    uint8_t *SdpInfo::avc_find_startcode(const uint8_t *p, const uint8_t *end)
    {
        uint8_t *out = avc_find_startcode_internal(p, end);
        if (p < out && out < end && !out[-1]) out--;
        return out;
    }

    uint8_t *SdpInfo::avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
    {
        const uint8_t *a = p + 4 - ((intptr_t)p & 3);

        for (end -= 3; p < a && p < end; p++) {
            if (p[0] == 0 && p[1] == 0 && p[2] == 1)
                return (uint8_t *)p;
        }

        for (end -= 3; p < end; p += 4) {
            uint32_t x = *(const uint32_t*)p;
            //      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
            //      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
            if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
                if (p[1] == 0) {
                    if (p[0] == 0 && p[2] == 1)
                        return (uint8_t *)p;
                    if (p[2] == 0 && p[3] == 1)
                        return (uint8_t *)p + 1;
                }
                if (p[3] == 0) {
                    if (p[2] == 0 && p[4] == 1)
                        return (uint8_t *)p + 2;
                    if (p[4] == 0 && p[5] == 1)
                        return (uint8_t *)p + 3;
                }
            }
        }

        for (end += 3; p < end; p++) {
            if (p[0] == 0 && p[1] == 0 && p[2] == 1)
                return (uint8_t *)p;
        }

        return (uint8_t *)end + 3;
    }

#define AV_BASE64_SIZE(x)  (((x)+2) / 3 * 4 + 1)

#ifndef AV_RB32
#   define AV_RB32(x)                                \
    (((uint32_t)((const uint8_t*)(x))[0] << 24) | \
    (((const uint8_t*)(x))[1] << 16) | \
    (((const uint8_t*)(x))[2] << 8) | \
    ((const uint8_t*)(x))[3])
#endif

    char *SdpInfo::base64_encode(char *out, int out_size, const uint8_t *in, int in_size)
    {
        static const char b64[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        char *ret, *dst;
        unsigned i_bits = 0;
        int i_shift = 0;
        int bytes_remaining = in_size;

        if ((uint32_t)in_size >= UINT_MAX / 4 ||
            out_size < AV_BASE64_SIZE(in_size))
            return NULL;
        ret = dst = out;
        while (bytes_remaining > 3) {
            i_bits = AV_RB32(in);
            in += 3; bytes_remaining -= 3;
            *dst++ = b64[i_bits >> 26];
            *dst++ = b64[(i_bits >> 20) & 0x3F];
            *dst++ = b64[(i_bits >> 14) & 0x3F];
            *dst++ = b64[(i_bits >> 8) & 0x3F];
        }
        i_bits = 0;
        while (bytes_remaining) {
            i_bits = (i_bits << 8) + *in++;
            bytes_remaining--;
            i_shift += 8;
        }
        while (i_shift > 0) {
            *dst++ = b64[(i_bits << 6 >> i_shift) & 0x3f];
            i_shift -= 6;
        }
        while ((dst - ret) & 3)
            *dst++ = '=';
        *dst = '\0';

        return ret;
    }

    static const uint8_t map2[256] =
    {
        0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff,

        0x3e, 0xff, 0xff, 0xff, 0x3f, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0xff,
        0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0x00, 0x01,
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
        0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1a, 0x1b,
        0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
        0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
        0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33,

        0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };

#define BASE64_DEC_STEP(i) do { \
    bits = map2[in[i]]; \
    if (bits & 0x80) \
    goto out ## i; \
    v = i ? (v << 6) + bits : bits; \
    } while (0)

#define AV_WL32(p, darg) do {                \
    unsigned d = (darg);                    \
    ((uint8_t*)(p))[0] = (d);               \
    ((uint8_t*)(p))[1] = (d) >> 8;            \
    ((uint8_t*)(p))[2] = (d) >> 16;           \
    ((uint8_t*)(p))[3] = (d) >> 24;           \
        } while (0)

#define AV_BSWAP16C(x) (((x) << 8 & 0xff00)  | ((x) >> 8 & 0x00ff))
#define AV_BSWAP32C(x) (AV_BSWAP16C(x) << 16 | AV_BSWAP16C((x) >> 16))
    int SdpInfo::base64_decode(uint8_t *out, const char *in_str, int out_size)
    {
        uint8_t *dst = out;
        uint8_t *end = out + out_size;
        // no sign extension
        const uint8_t *in = (const uint8_t *)in_str;
        unsigned bits = 0xff;
        unsigned v;

        while (end - dst > 3) {
            BASE64_DEC_STEP(0);
            BASE64_DEC_STEP(1);
            BASE64_DEC_STEP(2);
            BASE64_DEC_STEP(3);
            // Using AV_WB32 directly confuses compiler
            v = AV_BSWAP32C(v << 8);
            //AV_WNA(32, dst, v);
            AV_WL32(dst, v);
            dst += 3;
            in += 4;
        }
        if (end - dst) {
            BASE64_DEC_STEP(0);
            BASE64_DEC_STEP(1);
            BASE64_DEC_STEP(2);
            BASE64_DEC_STEP(3);
            *dst++ = v >> 16;
            if (end - dst)
                *dst++ = v >> 8;
            if (end - dst)
                *dst++ = v;
            in += 4;
        }
        while (1) {
            BASE64_DEC_STEP(0);
            in++;
            BASE64_DEC_STEP(0);
            in++;
            BASE64_DEC_STEP(0);
            in++;
            BASE64_DEC_STEP(0);
            in++;
        }

    out3:
        *dst++ = v >> 10;
        v <<= 2;
    out2:
        *dst++ = v >> 4;
    out1:
    out0 :
        return bits & 1 ? -1 : dst - out;
    }

    char *SdpInfo::data_to_hex(char *buff, const uint8_t *src, int s, int lowercase)
    {
        int i;
        static const char hex_table_uc[16] = { '0', '1', '2', '3',
            '4', '5', '6', '7',
            '8', '9', 'A', 'B',
            'C', 'D', 'E', 'F' };
        static const char hex_table_lc[16] = { '0', '1', '2', '3',
            '4', '5', '6', '7',
            '8', '9', 'a', 'b',
            'c', 'd', 'e', 'f' };
        const char *hex_table = lowercase ? hex_table_lc : hex_table_uc;

        for (i = 0; i < s; i++) {
            buff[i * 2] = hex_table[src[i] >> 4];
            buff[i * 2 + 1] = hex_table[src[i] & 0xF];
        }

        return buff;
    }

    static inline int toupper(int c)
    {
        if (c >= 'a' && c <= 'z')
            c ^= 0x20;
        return c;
    }

    int SdpInfo::hex_to_data(uint8_t *data, const char *p)
    {
        int c, len, v;

        len = 0;
        v = 1;
        for (;;) {
            p += strspn(p, SPACE_CHARS);
            if (*p == '\0')
                break;
            c = toupper((unsigned char)(*p));
            p++;
            if (c >= '0' && c <= '9')
                c = c - '0';
            else if (c >= 'A' && c <= 'F')
                c = c - 'A' + 10;
            else
                break;
            v = (v << 4) | c;
            if (v & 0x100) {
                if (data)
                    data[len] = v;
                len++;
                v = 1;
            }
        }
        return len;
    }

    int SdpInfo::strstart(const char *str, const char *pfx, const char **ptr)
    {
        while (*pfx && *pfx == *str) {
            pfx++;
            str++;
        }
        if (!*pfx && ptr)
            *ptr = str;
        return !*pfx;
    }

    int SdpInfo::isspace(int c)
    {
        return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' ||
            c == '\v';
    }

    void SdpInfo::get_word(char *buf, int buf_size, const char **pp)
    {
        const char *p;
        char *q;

        p = *pp;
        p += strspn(p, SPACE_CHARS);
        q = buf;
        while (!isspace(*p) && *p != '\0')
        {
            if ((q - buf) < buf_size - 1)
                *q++ = *p;
            p++;
        }
        if (buf_size > 0)
            *q = '\0';
        *pp = p;
    }


    int SdpInfo::parse_h264_sdp_line(rtp_media_info *s, const char *attr, const char *value)
    {
        if (!strcmp(attr, "packetization-mode"))
        {
            s->packetization_mode = atoi(value);
            /*
            * Packetization Mode:
            * 0 or not present: Single NAL mode (Only nals from 1-23 are allowed)
            * 1: Non-interleaved Mode: 1-23, 24 (STAP-A), 28 (FU-A) are allowed.
            * 2: Interleaved Mode: 25 (STAP-B), 26 (MTAP16), 27 (MTAP24), 28 (FU-A),
            *                      and 29 (FU-B) are allowed.
            */
            if (s->packetization_mode > 1)
            {
                WRN("Interleaved RTP mode is not supported yet.\n");
            }
        }
        else if (!strcmp(attr, "profile-level-id"))
        {
            if (strlen(value) == 6)
            {
                s->h264_profile_level_id = atoi(value);
            }
        }
        else if (!strcmp(attr, "sprop-parameter-sets"))
        {
            int ret;
            s->extra_data_len = 0;
            ret = h264_parse_sprop_parameter_sets(s, s->extra_data, s->extra_data_len, value);
            return ret;
        }
        return 0;
    }

    int SdpInfo::parse_aac_sdp_line(rtp_media_info *s, const char *attr, const char *value)
    {
        if (!strcmp(attr, "config"))
        {
            s->extra_data_len = hex_to_data(s->extra_data, value);
            return 0;
        } else if (!strcmp(attr, "constantDuration"))
        {
			s->constant_duration =  atoi(value);
        }
        return 0;
    }

    void SdpInfo::get_word_until_chars(char *buf, int buf_size,
        const char *sep, const char **pp)
    {
        const char *p;
        char *q;

        p = *pp;
        p += strspn(p, SPACE_CHARS);
        q = buf;
        while (!strchr(sep, *p) && *p != '\0') {
            if ((q - buf) < buf_size - 1)
                *q++ = *p;
            p++;
        }
        if (buf_size > 0)
            *q = '\0';
        *pp = p;
    }

    void SdpInfo::get_word_sep(char *buf, int buf_size, const char *sep,
        const char **pp)
    {
        if (**pp == '/') (*pp)++;
        get_word_until_chars(buf, buf_size, sep, pp);
    }



    int SdpInfo::rtp_next_attr_and_value(const char **p, char *attr, int attr_size,
        char *value, int value_size)
    {
        *p += strspn(*p, SPACE_CHARS);
        if (**p) {
            get_word_sep(attr, attr_size, "=", p);
            if (**p == '=')
                (*p)++;
            get_word_sep(value, value_size, ";", p);
            if (**p == ';')
                (*p)++;
            return 1;
        }
        return 0;
    }

    int SdpInfo::parse_fmtp(rtp_media_info *s, const char *p,
        int(*parse_fmtp_func)(rtp_media_info *s, const char *attr, const char *value))
    {
        char attr[256];
        char value[1024];
        int res;
        int value_size = strlen(p) + 1;

        // remove protocol identifier
        while (*p && *p == ' ')
            p++;                     // strip spaces
        //while (*p && *p != ' ')
        //    p++;                     // eat protocol identifier
       // while (*p && *p == ' ')
       //     p++;                     // strip trailing spaces

        while (rtp_next_attr_and_value(&p,
            attr, sizeof(attr),
            value, value_size)) {
            res = parse_fmtp_func(s, attr, value);
            if (res < 0)
            {
                return res;
            }
        }
        return 0;
    }
#define FF_INPUT_BUFFER_PADDING_SIZE 32
    static const uint8_t start_sequence[] = { 0, 0, 0, 1 };

    int SdpInfo::h264_parse_sprop_parameter_sets(rtp_media_info *s,
        uint8_t *data_ptr, uint32_t& data_size, const char *value)
    {
        int size = 0;

        while (*value)
        {
            char base64packet[1024];
            uint8_t decoded_packet[1024];
            int packet_size;
            char *dst = base64packet;

            while (*value && *value != ','
                && (dst - base64packet) < (uint32_t)sizeof(base64packet)-1)
            {
                *dst++ = *value++;
            }
            *dst++ = '\0';

            if (*value == ',')
                value++;

            packet_size = base64_decode(decoded_packet, base64packet, sizeof(decoded_packet));
            if (packet_size > 0)
            {
                if (size + sizeof(start_sequence)+packet_size > 2048)
                {
                    ERR("data_size is too large, size: %d", (int)(size + sizeof(start_sequence)+packet_size));
                    return -1;
                }
                uint32_t offset = size;

                memcpy(data_ptr + offset, start_sequence, sizeof(start_sequence));
                offset += sizeof(start_sequence);
                memcpy(data_ptr + offset, decoded_packet, packet_size);
                offset += packet_size;

                size = offset;
            }
        }

        data_size = size;

        return 0;
    }

}
