/**
* @file cache_manager_config.h
* @brief	This file define config for stream cache manager. \n
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/11/19
* @see  cache_manager.h
*/

#ifndef __CACHE_MANAGER_CONFIG_H_
#define __CACHE_MANAGER_CONFIG_H_

#include "../util/xml.h"
#include "config_manager.h"

namespace media_manager
{
    class FragmentGeneratorConfig
    {
    public:
        FragmentGeneratorConfig();

        virtual ~FragmentGeneratorConfig();

        FragmentGeneratorConfig& operator=(const FragmentGeneratorConfig& right);

        int32_t delay_time_msec;
    };

    class FLVFragmentGeneratorConfig : public FragmentGeneratorConfig
    {
    public:
        FLVFragmentGeneratorConfig();

        ~FLVFragmentGeneratorConfig();

        int32_t load_default_config();

        int32_t parse(struct xmlnode* input_config);

        FLVFragmentGeneratorConfig& operator=(const FLVFragmentGeneratorConfig& right);
    };

    class TSSegmentGeneratorConfig : public FragmentGeneratorConfig
    {
    public:
        TSSegmentGeneratorConfig();

        ~TSSegmentGeneratorConfig();

        int32_t load_default_config();

        int32_t parse(struct xmlnode* input_config);

        TSSegmentGeneratorConfig& operator=(const TSSegmentGeneratorConfig& right);

        int32_t target_duration_sec;
        int32_t duration_offset_sec;
    };

    class FLVBlockLiveCacheConfig : public FragmentGeneratorConfig
    {
    public:
        FLVBlockLiveCacheConfig();

        ~FLVBlockLiveCacheConfig();

        int32_t load_default_config();

        int32_t parse(struct xmlnode* input_config);

//        bool operator==(const FLVBlockLiveCacheConfig& right);

//        bool operator!=(const FLVBlockLiveCacheConfig& right);

        FLVBlockLiveCacheConfig& operator=(const FLVBlockLiveCacheConfig& right);

        int32_t buffer_duration_sec;
    };

    class FLVFragmentLiveCacheConfig
    {
    public:
        FLVFragmentLiveCacheConfig();

        ~FLVFragmentLiveCacheConfig();

        int32_t load_default_config();

        int32_t parse(struct xmlnode* input_config);

        FLVFragmentLiveCacheConfig& operator=(const FLVFragmentLiveCacheConfig& right);

        int32_t buffer_duration_sec;
    };

    class FLVFragmentTimeshiftCacheConfig
    {
    public:
        FLVFragmentTimeshiftCacheConfig();

        ~FLVFragmentTimeshiftCacheConfig();

        int32_t load_default_config();

        int32_t parse(struct xmlnode* input_config);

        FLVFragmentTimeshiftCacheConfig& operator=(const FLVFragmentTimeshiftCacheConfig& right);

        int32_t buffer_total_size;
        bool save_fragment_file_flag;
    };

    class FLVFragmentCacheConfig
    {
    public:
        FLVFragmentCacheConfig();

        ~FLVFragmentCacheConfig();

        int32_t load_default_config();

        int32_t parse(struct xmlnode* input_config);

        FLVFragmentCacheConfig& operator=(const FLVFragmentCacheConfig& right);

        FLVFragmentLiveCacheConfig* live_cache;
        FLVFragmentTimeshiftCacheConfig* timeshift_cache;
    };

    class HLSM3U8LiveCacheConfig
    {
    public:
        HLSM3U8LiveCacheConfig();

        int32_t load_default_config();

        int32_t parse(struct xmlnode* input_config);

        HLSM3U8LiveCacheConfig& operator=(const HLSM3U8LiveCacheConfig& right);

        int32_t max_segment_num;
        int32_t real_segment_num;
    };

    class HLSTSLiveCacheConfig
    {
    public:
        HLSTSLiveCacheConfig();

        ~HLSTSLiveCacheConfig();

        int32_t load_default_config();

        int32_t parse(struct xmlnode* input_config);

        HLSTSLiveCacheConfig& operator=(const HLSTSLiveCacheConfig& right);

        int32_t buffer_duration_sec;
    };

    class HLSCacheConfig
    {
    public:
        HLSCacheConfig();

        int32_t load_default_config();

        int32_t parse(struct xmlnode* input_config);

        HLSCacheConfig& operator=(const HLSCacheConfig& right);

        HLSM3U8LiveCacheConfig* m3u8_live_cache;
        HLSTSLiveCacheConfig* ts_live_cache;

        ~HLSCacheConfig();
    };

    class CacheManagerConfig : public ConfigModule
    {
    public:
        CacheManagerConfig();

        void init(bool enable_generator = false);

        virtual ~CacheManagerConfig();

        int32_t load_default_config();

        int32_t parse(struct xmlnode* input_config);

        CacheManagerConfig& operator=(const CacheManagerConfig& right);

    public:
        virtual void set_default_config();

        virtual bool load_config(xmlnode* xml_config);

        virtual bool reload() const;

        virtual const char* module_name() const;

        virtual void dump_config() const;

    public:
        //FLVFragmentGeneratorConfig* flv_fragment_generator;

        //TSSegmentGeneratorConfig* ts_segment_generator;

        FLVBlockLiveCacheConfig* flv_block_live_cache;

        //FLVFragmentCacheConfig* flv_fragment_cache;

        //HLSCacheConfig* hls_cache;

        char save_stream_dir[4096];

        int32_t live_push_timeout_sec;

        int32_t live_req_timeout_sec;

    };
};

#endif  /* __CACHE_MANAGER_CONFIG_H_ */
