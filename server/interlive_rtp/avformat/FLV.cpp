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


#include "FLV.h"
#include "../util/log.h"
#include "AMF.h"


#ifdef WIN32
#include <Windows.h>
#else
#include <netinet/in.h>
#endif


#define AVC(str)	{str,sizeof(str)-1}
#define SAVC(x)    static const AVal av_##x = AVC(#x)

using namespace  avcodec;

FLVHeader::FLVHeader()
{
    _full_header = NULL;
    _flv_flag = 0;
    _buffer = buffer_init(4096);
}

FLVHeader::~FLVHeader()
{
    if (_full_header != NULL)
    {
        delete [] _full_header;
    }

    if (_buffer != NULL)
    {
        buffer_free(_buffer);
        _buffer = NULL;
    }
}

flv_header* FLVHeader::get_header(int32_t& len)
{
    if (buffer_data_len(_buffer) == 0)
    {
        len = 0;
        return NULL;
    }

    len = buffer_data_len(_buffer);
    return (flv_header*)buffer_data_ptr(_buffer);
}

int FLVHeader::build(const uint8_t *avc_config, uint32_t avc_config_len,
    const uint8_t* aac_config, int32_t aac_config_len,
    int32_t& status_code)
{
    // aac0 8
    flv_tag* aac0_tag = NULL;
    int aac0_tag_size = 0;

    if (aac_config != NULL && aac_config_len > 0)
    {
        aac0_tag = (flv_tag*)new char[2048];
        generate_aac0_tag(aac_config, aac_config_len, aac0_tag, aac0_tag_size, status_code);
    }

    // avc0 9
    flv_tag* avc0_tag = NULL;
    int avc0_tag_size = 0;

    if (avc_config != NULL && avc_config_len > 0)
    {
        avc0_tag = (flv_tag*)new char[2048];
        generate_avc0_tag(avc_config, avc_config_len, avc0_tag, avc0_tag_size, status_code);
    }

    // flv_header
    flv_header header;
    header.signature[0] = 'F';
    header.signature[1] = 'L';
    header.signature[2] = 'V';
    header.version = 1;
    header.flags = _flv_flag;
    header.offset[0] = 0;
    header.offset[1] = 0;
    header.offset[2] = 0;
    header.offset[3] = 9;

    buffer_append_ptr(_buffer, &header, sizeof(header));

    uint8_t pre_tag_size_str[4];
    FLV::flv_set_pretagsize(pre_tag_size_str, 0);
    buffer_append_ptr(_buffer, pre_tag_size_str, sizeof(pre_tag_size_str));

    // metadata tag 18
    flv_tag* metadata_tag = NULL;
    int metadata_tag_size = 0;

    metadata_tag = (flv_tag*)new char[2048];
    generate_metadata_tag(avc_config, avc_config_len, aac_config, aac_config_len, metadata_tag, metadata_tag_size, status_code);

    buffer_append_ptr(_buffer, metadata_tag, metadata_tag_size);

    delete [] metadata_tag;

    if (aac0_tag != NULL)
    {
        buffer_append_ptr(_buffer, aac0_tag, aac0_tag_size);
        delete [] aac0_tag;
    }

    if (avc0_tag != NULL)
    {
        buffer_append_ptr(_buffer, avc0_tag, avc0_tag_size);
        delete [] avc0_tag;
    }

    if (_full_header != NULL)
    {
        delete _full_header;
    }

    _full_header = (flv_header*)new char[buffer_data_len(_buffer)];
    memcpy(_full_header, buffer_data_ptr(_buffer), buffer_data_len(_buffer));

    return 0;
}

int FLVHeader::generate_aac0_tag(const uint8_t *payload, uint32_t payload_len,
    flv_tag* tag, int32_t& tag_len, int32_t& status)
{
    int len = 0;

    tag->type = FLV_TAG_AUDIO;
    FLV::flv_set_timestamp(tag->timestamp, 0);
    FLV::flv_set_streamid(tag->streamid, 0);

    int data_size = 0;
    int ret = generate_aac0_data(payload, payload_len, tag->data, data_size, status);
    if (ret >= 0)
    {
        FLV::flv_set_datasize(tag->datasize, data_size);
        int pre_tag_size = data_size + sizeof(flv_tag);
        FLV::flv_set_pretagsize(tag->data + data_size, pre_tag_size);
        len = pre_tag_size + 4;
        tag_len = len;
        return 0;
    }
    else
    {
        return -1;
    }
}

int FLVHeader::generate_avc0_tag(const uint8_t *payload, uint32_t payload_len,
    flv_tag* tag, int32_t& tag_len, int32_t& status)
{
    int len = 0;

    tag->type = FLV_TAG_VIDEO;
    FLV::flv_set_timestamp(tag->timestamp, 0);
    FLV::flv_set_streamid(tag->streamid, 0);

    int data_size = 0;
    int ret = generate_avc0_data(payload, payload_len, tag->data, data_size, status);
    if (ret >= 0)
    {
        FLV::flv_set_datasize(tag->datasize, data_size);
        int pre_tag_size = data_size + sizeof(flv_tag);
        FLV::flv_set_pretagsize(tag->data + data_size, pre_tag_size);
        len = pre_tag_size + 4;
        tag_len = len;
        return 0;
    }
    else
    {
        return -1;
    }
}

int FLVHeader::generate_metadata_tag(
    const uint8_t* avc_config, uint32_t avc_config_len,
    const uint8_t* aac_config, int32_t aac_config_len,
    flv_tag* tag, int32_t& tag_len, int32_t& status)
{
    tag->type = FLV_TAG_SCRIPT;
    FLV::flv_set_timestamp(tag->timestamp, 0);
    FLV::flv_set_streamid(tag->streamid, 0);

    char* enc = (char*)tag->data;
    char* pend = (char*)tag + 2048;

    SAVC(onMetaData);
    enc = AMF_EncodeString(enc, pend, &av_onMetaData);


    CH264ConfigParser parser;

    if (avc_config && avc_config_len > 0)
    {
        const uint8_t *end = avc_config + avc_config_len;
        const uint8_t *r;

        r = ff_avc_find_startcode(avc_config, end);

        while (r < end)
        {
            const uint8_t *r1;
            while (!*(r++));
            r1 = ff_avc_find_startcode(r, end);
            int nalu_type = *(r)& 0x1f;
            if (nalu_type == NAL_SPS)
            {
                parser.ParseSPS(r, r1 - r);
            }
            r = r1;
        }
    }


    AACConfig a_config;
    if (aac_config && aac_config_len > 0)
    {
        a_config.parse_aac_specific((uint8_t*)aac_config, aac_config_len);
    }


    char *endMetaData = EncMetaData(enc, pend, false, parser.getWidth(), parser.getHeight(),
        a_config.get_samplingFrequency(), a_config.get_channelConfig());
    int metadata_size = endMetaData - (char*)tag->data;

    FLV::flv_set_datasize(tag->datasize, metadata_size);
    int pre_tag_size = metadata_size + sizeof(flv_tag);
    FLV::flv_set_pretagsize(tag->data + metadata_size, pre_tag_size);
    tag_len = pre_tag_size + 4;

    return 0;
}

int FLVHeader::generate_aac0_data(const uint8_t* aac_config, int32_t aac_config_len,
    uint8_t* output, int32_t& output_len, int32_t& status)
{
    if (output == NULL)
    {
        // TODO error!
        return -1;
    }

    if ((aac_config == NULL) || (aac_config_len <= 0))
    {
        return -1;
    }

    output[0] = 0xaf;
    output[1] = 0x00;

    memcpy(output + 2, aac_config, aac_config_len);
    output_len = aac_config_len + 2;

    _flv_flag |= FLV_FLAG_AUDIO;

    return 0;
}

int FLVHeader::generate_avc0_data(const uint8_t *avc_config, uint32_t avc_config_len,
    uint8_t* output, int32_t& output_len, int32_t& status)
{
    if (!avc_config || avc_config_len <= 0)
    {
        // TODO error
        return -1;
    }

    uint8_t* sps = new uint8_t[1024];
    uint8_t* pps = new uint8_t[1024];
    int sps_len;
    int pps_len;

    buffer *buf = buffer_init(2048);

    //FIXME convert decode avcconfig
    //H264::nalu2pset(nal_list, nalNum, sps, sps_len, pps, pps_len, status);

    const uint8_t *end = avc_config + avc_config_len;
    const uint8_t *r;

    r = ff_avc_find_startcode(avc_config, end);

    while (r < end)
    {
        const uint8_t *r1;
        while (!*(r++));
        r1 = ff_avc_find_startcode(r, end);
        int nalu_type = *(r) & 0x1f;
        if (nalu_type == NAL_SPS)
        {
            memcpy(sps, r, r1 - r);
            sps_len = r1 - r;
        }
        else if (nalu_type == NAL_PPS)
        {
            memcpy(pps, r, r1 - r);
            pps_len = r1 - r;
        }
        r = r1;
    }

    if (sps == NULL || pps == NULL || sps_len == 0 || pps_len == 0)
    {
        delete [] sps;
        delete [] pps;
        buffer_free(buf);
        return -1;
    }

    buffer_append_byte(buf, 0x17);
    buffer_append_byte(buf, 0);
    buffer_append_byte(buf, 0);
    buffer_append_byte(buf, 0);
    buffer_append_byte(buf, 0);
    buffer_append_byte(buf, 1);
    buffer_append_ptr(buf, sps + 1, 3);
    buffer_append_byte(buf, 0xff);
    buffer_append_byte(buf, 0xe1);
    int t_sps_len = htons(sps_len);
    buffer_append_ptr(buf, &t_sps_len, 2);
    buffer_append_ptr(buf, sps, sps_len);

    buffer_append_byte(buf, 1);
    int t_pps_len = htons(pps_len);
    buffer_append_ptr(buf, &t_pps_len, 2);
    buffer_append_ptr(buf, pps, pps_len);

    _flv_flag |= FLV_FLAG_VIDEO;

    memcpy(output, buffer_data_ptr(buf), buffer_data_len(buf));
    output_len = buffer_data_len(buf);

    //delete buf;
    buffer_free(buf);
    delete [] sps;
    delete [] pps;

    return 0;
}

char* FLVHeader::EncMetaData(char *enc, char *pend, bool bFLVFile, int width, int height, int audioSampleRate, int audioChannels)
{
    SAVC(avc1);
    int maxBitRate = 500;
    int fps = 15;

    //SAVC(mp4a);
    int audioBitRate = 64;
    int audioSampleSize = 16;
    static const AVal av_Version = AVC("LiveStreamSDK 0.1");

    *enc++ = AMF_ECMA_ARRAY;
    enc = AMF_EncodeInt32(enc, pend, 14);

    SAVC(duration);
    SAVC(fileSize);
    SAVC(width);
    SAVC(height);
    SAVC(videocodecid);
    SAVC(videodatarate);
    SAVC(framerate);
    SAVC(audiocodecid);
    SAVC(audiodatarate);
    SAVC(audiosamplerate);
    SAVC(audiosamplesize);
    SAVC(stereo);
    SAVC(encoder);

    enc = AMF_EncodeNamedNumber(enc, pend, &av_duration, 0.0);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_fileSize, 0.0);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_width, double(width));
    enc = AMF_EncodeNamedNumber(enc, pend, &av_height, double(height));

    enc = AMF_EncodeNamedString(enc, pend, &av_videocodecid, &av_avc1);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_videodatarate, double(maxBitRate));
    enc = AMF_EncodeNamedNumber(enc, pend, &av_framerate, double(fps));

    //enc = AMF_EncodeNamedString(enc, pend, &av_audiocodecid, &av_mp4a);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_audiocodecid, double(16));

    enc = AMF_EncodeNamedNumber(enc, pend, &av_audiodatarate, double(audioBitRate));
    enc = AMF_EncodeNamedNumber(enc, pend, &av_audiosamplerate, double(audioSampleRate));
    enc = AMF_EncodeNamedNumber(enc, pend, &av_audiosamplesize, double(audioSampleSize));

    enc = AMF_EncodeNamedBoolean(enc, pend, &av_stereo, audioChannels == 2);
    enc = AMF_EncodeNamedString(enc, pend, &av_encoder, &av_Version);

    *enc++ = 0;
    *enc++ = 0;
    *enc++ = AMF_OBJECT_END;

    return enc;
}

FLV::FLV()
{
    _temp_tag = (flv_tag*)new char[1024 * 1024];
    _initial_audio_ts = -1;
    _initial_video_ts = -1;
}

FLV::~FLV()
{
    delete[] _temp_tag;
}

flv_tag* FLV::generate_audio_tag(
    const uint8_t* aac_payload, uint32_t aac_payload_len,
    int32_t timestamp,
    int32_t& tag_len, int32_t& status)
{
    if (_initial_audio_ts == -1)
    {
        _initial_audio_ts = timestamp;
    }

    timestamp -= _initial_audio_ts;

    _temp_tag->type = FLV_TAG_AUDIO;
    FLV::flv_set_timestamp(_temp_tag->timestamp, timestamp);
    FLV::flv_set_streamid(_temp_tag->streamid, 0);

    int data_size = 0;
    int ret = generate_audio_tag_data(aac_payload, aac_payload_len, _temp_tag->data, data_size, status);
    if (ret >= 0)
    {
        FLV::flv_set_datasize(_temp_tag->datasize, data_size);
        int pre_tag_size = data_size + sizeof(flv_tag);
        FLV::flv_set_pretagsize(_temp_tag->data + data_size, pre_tag_size);
        tag_len = pre_tag_size + 4;
        return _temp_tag;
    }
    else
    {
        return NULL;
    }
}

int32_t FLV::generate_audio_tag_data(const uint8_t* aac_payload, uint32_t aac_payload_len,
    uint8_t* tag_data, int32_t& data_len, int32_t& status)
{
    int32_t offset = 0;
    *(tag_data + offset) = 0xaf;
    offset++;
    *(tag_data + offset) = 0x01;
    offset++;
    memcpy(tag_data + offset, aac_payload, aac_payload_len);
    offset += aac_payload_len;
    data_len = offset;
    return 0;
}

flv_tag* FLV::generate_video_tag(
    const uint8_t *payload, uint32_t payload_len,
    int32_t timestamp,
    int32_t& tag_len, int32_t& status)
{
    if (_initial_video_ts == -1)
    {
        _initial_video_ts = timestamp;
    }

    timestamp -= _initial_video_ts;

    _temp_tag->type = FLV_TAG_VIDEO;
    FLV::flv_set_timestamp(_temp_tag->timestamp, timestamp);
    FLV::flv_set_streamid(_temp_tag->streamid, 0);

    int data_size = 0;
    int ret = generate_video_tag_data(payload, payload_len, _temp_tag->data, data_size, status);
    if (ret >= 0)
    {
        FLV::flv_set_datasize(_temp_tag->datasize, data_size);
        int pre_tag_size = data_size + sizeof(flv_tag);
        FLV::flv_set_pretagsize(_temp_tag->data + data_size, pre_tag_size);
        tag_len = pre_tag_size + 4;
        return _temp_tag;
    }
    else
    {
        return NULL;
    }
}

int32_t FLV::generate_video_tag_data(const uint8_t *payload, uint32_t payload_len,
    uint8_t* tag_data, int32_t& data_len, int32_t& status)
{
    bool bFoundFrame = false;

    int32_t offset = 5;

    //for (int i = 0; i < nalNum; i++)
    //{
    const uint8_t *end = payload + payload_len;
    const uint8_t *r = payload;

    if (*(r) == 0 && *(r + 1) == 0 && *(r + 2) == 0 && *(r + 3) == 1)
    {
        r = ff_avc_find_startcode(payload, end);
        while (r < end)
        {
            const uint8_t *r1;
            while (!*(r++));
            r1 = ff_avc_find_startcode(r, end);

            int newPayloadSize = r1 - r;

            int nalu_type = *(r)& 0x1f;

            if (nalu_type != NAL_SPS && nalu_type != NAL_PPS)
            {
                if (!bFoundFrame)
                {
                    if (nalu_type == NAL_SLICE_IDR)
                    {
                        *(tag_data) = 0x17;
                    }
                    else
                    {
                        *(tag_data) = 0x27;
                    }
                    *(tag_data + 1) = 0x01;
                    *(tag_data + 2) = 0x00;
                    *(tag_data + 3) = 0x00;
                    *(tag_data + 4) = 0x00;

                    bFoundFrame = true;
                }
                *((uint32_t *)(tag_data + offset)) = htonl(newPayloadSize);
                offset += 4;
                memcpy(tag_data + offset, r, newPayloadSize);
                offset += newPayloadSize;
            }

            r = r1;
        }
    }
    else {
        r = payload;
        while (r < end)
        {
            uint32_t nalu_size = ntohl(*((uint32_t *)r));
            if (r + nalu_size > end)
            {
                DBG("nalu size error %u,skip",nalu_size);
                break;
            }
            uint8_t nalu_type = *(r + 4) & 0x1f;
            r += 4;
            if (nalu_type != avcodec::NAL_SPS && nalu_type != avcodec::NAL_PPS)
            {
                if (!bFoundFrame)
                {
                    if (nalu_type == NAL_SLICE_IDR)
                    {
                        *(tag_data) = 0x17;
                    }
                    else
                    {
                        *(tag_data) = 0x27;
                    }
                    *(tag_data + 1) = 0x01;
                    *(tag_data + 2) = 0x00;
                    *(tag_data + 3) = 0x00;
                    *(tag_data + 4) = 0x00;

                    bFoundFrame = true;
                }
                *((uint32_t *)(tag_data + offset)) = htonl(nalu_size);
                offset += 4;
                memcpy(tag_data + offset, r, nalu_size);
                offset += nalu_size;
            }
            r += nalu_size;
        }
    }


    data_len = offset;
    return 0;
}

//x264_nal_t* FLV::get_x264_nal(flv_tag* tag, int32_t& nal_num)
//{
//    if (tag->type != FLV_TAG_VIDEO)
//    {
//        return NULL;
//    }
//
//    if (FLV::flv_is_avc0(tag->data))
//    {
//        nal_num = 2;
//        x264_nal_t* nal_list = new x264_nal_t[2];
//
//        int tag_data_offset = 11;
//        x264_nal_t& sps_nal = nal_list[0];
//        uint16_t sps_len = (tag->data[tag_data_offset] << 8) + (tag->data[tag_data_offset + 1]);
//
//        tag_data_offset += 2; // sps_len is uint16.
//        sps_nal.i_type = tag->data[tag_data_offset] & 0x0f;
//        sps_nal.i_payload = sps_len + 4;
//        sps_nal.p_payload = new uint8_t[sps_nal.i_payload];
//        sps_nal.p_payload[0] = 0;
//        sps_nal.p_payload[1] = 0;
//        sps_nal.p_payload[2] = 0;
//        sps_nal.p_payload[3] = 1;
//
//        memcpy(sps_nal.p_payload + 4, tag->data + tag_data_offset, sps_len);
//
//        tag_data_offset += sps_len + 1;
//        x264_nal_t& pps_nal = nal_list[1];
//        uint16_t pps_len = (tag->data[tag_data_offset] << 8) + (tag->data[tag_data_offset + 1]);
//
//        tag_data_offset += 2; // pps_len is uint16.
//        pps_nal.i_type = tag->data[tag_data_offset] & 0x0f;
//        pps_nal.i_payload = pps_len + 4;
//        pps_nal.p_payload = new uint8_t[pps_nal.i_payload];
//        pps_nal.p_payload[0] = 0;
//        pps_nal.p_payload[1] = 0;
//        pps_nal.p_payload[2] = 0;
//        pps_nal.p_payload[3] = 1;
//
//        memcpy(pps_nal.p_payload + 4, tag->data + tag_data_offset, pps_len);
//
//        return nal_list;
//    }
//    else
//    {
//        nal_num = 0;
//
//        int32_t data_len = FLV::flv_get_datasize(tag->datasize);
//
//        int tag_data_offset = 5;
//
//        while (tag_data_offset < data_len)
//        {
//            int i_payload = H264::nal_slice_get_size(tag->data + tag_data_offset);
//            tag_data_offset += 4;
//            tag_data_offset += i_payload;
//            nal_num++;
//        }
//
//        if (nal_num == 0)
//        {
//            return NULL;
//        }
//
//        x264_nal_t* nal_list = new x264_nal_t[nal_num];
//
//        tag_data_offset = 5;
//
//        int i = 0;
//        int slice_idx = -1;
//        while (tag_data_offset < data_len)
//        {
//            x264_nal_t& nal = nal_list[i];
//            int payload_len = H264::nal_slice_get_size(tag->data + tag_data_offset);
//
//            tag_data_offset += 4;
//            nal.i_type = tag->data[tag_data_offset] & 0x0f;
//
//            int start_code_len = 4;
//
//            if (nal.i_type == NAL_SLICE || nal.i_type == NAL_SLICE_IDR)
//            {
//                slice_idx++;
//                if (slice_idx > 0)
//                {
//                    start_code_len = 3;
//                }
//            }
//
//            nal.i_payload = payload_len + start_code_len;
//
//            nal.p_payload = new uint8_t[nal.i_payload];
//            nal.p_payload[0] = 0;
//            nal.p_payload[1] = 0;
//            
//            if (start_code_len == 3)
//            {
//                nal.p_payload[2] = 1;
//            }
//
//            if (start_code_len == 4)
//            {
//                nal.p_payload[2] = 0;
//                nal.p_payload[3] = 1;
//            }
//
//            memcpy(nal.p_payload + start_code_len, tag->data + tag_data_offset, payload_len);
//            tag_data_offset += payload_len;
//
//            i++;
//        }
//
//        return nal_list;
//    }
//
//}
//
//uint8_t* FLV::get_aac_payload(flv_tag* tag, int32_t& len)
//{
//    if (tag->type != FLV_TAG_AUDIO)
//    {
//        return NULL;
//    }
//
//    int tag_len = FLV::flv_get_datasize(tag->datasize);
//    len = tag_len - 2;
//    uint8_t* aac_data = new uint8_t[len];
//    memcpy(aac_data, tag->data + 2, len);
//
//    return aac_data;
//}
