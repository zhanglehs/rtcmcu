/**
* @file cache_manager_config.cpp
* @brief	This file define config for stream cache manager. \n
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/11/19
* @see  cache_manager_config.h cache_manager.h
*/

#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <iostream>

#include "util/log.h"
#include "util/xml.h"
#include "cache_manager_config.h"
#include "cache_manager.h"

using namespace std;

namespace media_manager
{
    FragmentGeneratorConfig::FragmentGeneratorConfig()
        :delay_time_msec(0)
    {

    }

    FragmentGeneratorConfig::~FragmentGeneratorConfig()
    {

    }

    FragmentGeneratorConfig& FragmentGeneratorConfig::operator
        = (const FragmentGeneratorConfig& right)
    {
        delay_time_msec = right.delay_time_msec;
        return *this;
    }

	/*
    FLVFragmentGeneratorConfig::FLVFragmentGeneratorConfig()
    {

    }

    FLVFragmentGeneratorConfig::~FLVFragmentGeneratorConfig()
    {

    }

    int32_t FLVFragmentGeneratorConfig::load_default_config()
    {
        delay_time_msec = 500;
        return 0;
    }

    int32_t FLVFragmentGeneratorConfig::parse(struct xmlnode* input_config)
    {
        if (input_config == NULL)
        {
            WRN("can not find flv_fragment_generator config, use default config");
            load_default_config();
            return 0;
        }

        char* attr = xmlgetattr(input_config, "delay_time_msec");

        if (!attr)
        {
            WRN("parse flv_fragment_generator.delay_time_msec failed.");
        }
        else
        {
            delay_time_msec = atoi(attr);
        }

        return 0;
    }

    FLVFragmentGeneratorConfig& FLVFragmentGeneratorConfig::operator
        = (const FLVFragmentGeneratorConfig& right)
    {
        FragmentGeneratorConfig* base = dynamic_cast<FragmentGeneratorConfig*>(this);
        *base = right;

        return *this;
    }



    TSSegmentGeneratorConfig::TSSegmentGeneratorConfig()
        :target_duration_sec(8),
        duration_offset_sec(5)
    {
        load_default_config();
    }

    TSSegmentGeneratorConfig::~TSSegmentGeneratorConfig()
    {

    }

    int32_t TSSegmentGeneratorConfig::load_default_config()
    {
        delay_time_msec = 500;
        target_duration_sec = 8;
        duration_offset_sec = 5;
        
        return 0;
    }

    int32_t TSSegmentGeneratorConfig::parse(struct xmlnode* input_config)
    {
        if (input_config == NULL)
        {
            WRN("can not find ts_fragment_generator config, use default config");
            load_default_config();
            return 0;
        }

        char* attr = xmlgetattr(input_config, "delay_time_msec");

        if (!attr)
        {
            WRN("parse flv_fragment_generator.delay_time_msec failed.");
        }
        else
        {
            delay_time_msec = atoi(attr);
        }

        attr = xmlgetattr(input_config, "target_duration_sec");

        if (!attr)
        {
            WRN("parse ts_fragment_generator.target_duration_sec failed.");
        }
        else
        {
            target_duration_sec = atoi(attr);
        }
        
        attr = xmlgetattr(input_config, "duration_offset_sec");

        if (!attr)
        {
            WRN("parse ts_fragment_generator.duration_offset_sec failed.");
        }
        else
        {
            duration_offset_sec = atoi(attr);
        }

        return 0;
    }

    TSSegmentGeneratorConfig& TSSegmentGeneratorConfig::operator 
        = (const TSSegmentGeneratorConfig& right)
    {
        FragmentGeneratorConfig* base = dynamic_cast<FragmentGeneratorConfig*>(this);
        *base = right;

        target_duration_sec = right.target_duration_sec;
        duration_offset_sec = right.duration_offset_sec;

        return *this;
    }
	*/
    FLVBlockLiveCacheConfig::FLVBlockLiveCacheConfig()
        :buffer_duration_sec(60)
    {
    }

    FLVBlockLiveCacheConfig::~FLVBlockLiveCacheConfig()
    {

    }

    int32_t FLVBlockLiveCacheConfig::load_default_config()
    {
        buffer_duration_sec = 60;

        return 0;
    }

    int32_t FLVBlockLiveCacheConfig::parse(struct xmlnode* input_config)
    {
        if (input_config == NULL)
        {
            WRN("can not find flv_block_live_cache config, use default config");
            load_default_config();
            return 0;
        }

        char* attr = xmlgetattr(input_config, "buffer_duration_sec");

        if (!attr)
        {
            ERR("parse flv_block_live_cache.buffer_duration_sec failed.");
        }
        else
        {
            buffer_duration_sec = atoi(attr);
        }

        return 0;
    }

    FLVBlockLiveCacheConfig& FLVBlockLiveCacheConfig::operator = (const FLVBlockLiveCacheConfig& right)
    {
        this->buffer_duration_sec = right.buffer_duration_sec;
        return *this;
    }

	/*
    FLVFragmentLiveCacheConfig::FLVFragmentLiveCacheConfig()
        :buffer_duration_sec(60)
    {
    }

    FLVFragmentLiveCacheConfig::~FLVFragmentLiveCacheConfig()
    {

    }

    int32_t FLVFragmentLiveCacheConfig::load_default_config()
    {
        buffer_duration_sec = 60;

        return 0;
    }

    int32_t FLVFragmentLiveCacheConfig::parse(struct xmlnode* input_config)
    {
        if (input_config == NULL)
        {
            WRN("can not find flv_fragment_live_cache config, use default config");
            load_default_config();
            return 0;
        }

        char* attr = xmlgetattr(input_config, "buffer_duration_sec");

        if (!attr)
        {
            ERR("parse flv_fragment_live_cache.buffer_duration_sec failed.");
            return -1;
        }

        buffer_duration_sec = atoi(attr);

        return 0;
    }

    FLVFragmentLiveCacheConfig& FLVFragmentLiveCacheConfig::operator 
        = (const FLVFragmentLiveCacheConfig& right)
    {
        this->buffer_duration_sec = right.buffer_duration_sec;
        return *this;
    }


    FLVFragmentTimeshiftCacheConfig::FLVFragmentTimeshiftCacheConfig()
        :buffer_total_size(60),
        save_fragment_file_flag(false)
    {
    }

    FLVFragmentTimeshiftCacheConfig::~FLVFragmentTimeshiftCacheConfig()
    {

    }

    int32_t FLVFragmentTimeshiftCacheConfig::load_default_config()
    {
        buffer_total_size = 60;
        save_fragment_file_flag = false;

        return 0;
    }

    int32_t FLVFragmentTimeshiftCacheConfig::parse(struct xmlnode* input_config)
    {
        if (input_config == NULL)
        {
            WRN("can not find flv_fragment_timeshift_cache config, use default config");
            load_default_config();
            return 0;
        }

        char* attr = xmlgetattr(input_config, "buffer_total_size");

        if (!attr)
        {
            ERR("parse flv_fragment_timeshift_cache.buffer_total_size failed.");
            return -1;
        }

        buffer_total_size = atoi(attr);

        attr = xmlgetattr(input_config, "save_fragment_file_flag");
        if (!attr)
        {
            ERR("parse flv_fragment_timeshift_cache.save_fragment_file_flag config failed.");
            return -1;
        }

        if (strcasecmp(attr, "true") == 0)
        {
            save_fragment_file_flag = true;
        }
        else
        {
            save_fragment_file_flag = false;
        }

        return 0;
    }

    FLVFragmentTimeshiftCacheConfig& FLVFragmentTimeshiftCacheConfig::operator
        = (const FLVFragmentTimeshiftCacheConfig& right)
    {
        this->buffer_total_size = right.buffer_total_size;
        this->save_fragment_file_flag = right.save_fragment_file_flag;
        return *this;
    }


    FLVFragmentCacheConfig::FLVFragmentCacheConfig()
        :live_cache(NULL),
        timeshift_cache(NULL)
    {
        live_cache = new FLVFragmentLiveCacheConfig();
        timeshift_cache = new FLVFragmentTimeshiftCacheConfig();
    }

    FLVFragmentCacheConfig::~FLVFragmentCacheConfig()
    {
        if (live_cache != NULL)
        {
            delete live_cache;
            live_cache = NULL;
        }

        if (timeshift_cache != NULL)
        {
            delete timeshift_cache;
            timeshift_cache = NULL;
        }
    }

    int32_t FLVFragmentCacheConfig::load_default_config()
    {
        live_cache->load_default_config();
        timeshift_cache->load_default_config();

        return 0;
    }

    int32_t FLVFragmentCacheConfig::parse(struct xmlnode* input_config)
    {
        struct xmlnode* node = NULL;
        int32_t ret1, ret2;

        if (input_config == NULL)
        {
            WRN("can not find flv_fragment_cache config, use default config");
            load_default_config();
            return 0;
        }

        node = xmlgetchild(input_config, "live_cache", 0);
        ret1 = live_cache->parse(node);

        node = xmlgetchild(input_config, "timeshift_cache", 0);
        ret2 = timeshift_cache->parse(node);

        if (ret1 < 0 || ret2 < 0)
        {
            return -1;
        }

        return 0;
    }

    FLVFragmentCacheConfig& FLVFragmentCacheConfig::operator
        = (const FLVFragmentCacheConfig& right)
    {
        *live_cache = *(right.live_cache);
        *timeshift_cache = *(right.timeshift_cache);

        return *this;
    }


    HLSM3U8LiveCacheConfig::HLSM3U8LiveCacheConfig()
        :max_segment_num(3),
        real_segment_num(2)
    {

    }

    int32_t HLSM3U8LiveCacheConfig::load_default_config()
    {
        max_segment_num = 3;
        real_segment_num = 2;

        return 0;
    }

    int32_t HLSM3U8LiveCacheConfig::parse(struct xmlnode* input_config)
    {
        if (input_config == NULL)
        {
            WRN("can not find m3u8_live_cache config, use default config");
            load_default_config();
            return 0;
        }

        char* attr = xmlgetattr(input_config, "max_segment_num");

        if (!attr)
        {
            ERR("parse m3u8_live_cache.max_segment_num failed.");
            return -1;
        }

        max_segment_num = atoi(attr);

        attr = xmlgetattr(input_config, "real_segment_num");

        if (!attr)
        {
            ERR("parse m3u8_live_cache.real_segment_num failed.");
            return -1;
        }

        real_segment_num = atoi(attr);

        return 0;
    }

    HLSM3U8LiveCacheConfig& HLSM3U8LiveCacheConfig::operator
        = (const HLSM3U8LiveCacheConfig& right)
    {
        max_segment_num = right.max_segment_num;
        real_segment_num = right.real_segment_num;

        return *this;
    }


    HLSTSLiveCacheConfig::HLSTSLiveCacheConfig()
        :buffer_duration_sec(60)
    {

    }

    HLSTSLiveCacheConfig::~HLSTSLiveCacheConfig()
    {

    }

    int32_t HLSTSLiveCacheConfig::load_default_config()
    {
        buffer_duration_sec = 60;

        return 0;
    }

    int32_t HLSTSLiveCacheConfig::parse(struct xmlnode* input_config)
    {
        if (input_config == NULL)
        {
            WRN("can not find ts_live_cache config, use default config");
            load_default_config();
            return 0;
        }

        char* attr = xmlgetattr(input_config, "buffer_duration_sec");

        if (!attr)
        {
            ERR("parse ts_live_cache.buffer_duration_sec failed.");
            return -1;
        }

        buffer_duration_sec = atoi(attr);

        return 0;
    }

    HLSTSLiveCacheConfig& HLSTSLiveCacheConfig::operator
        = (const HLSTSLiveCacheConfig& right)
    {
        buffer_duration_sec = right.buffer_duration_sec;

        return *this;
    }


    HLSCacheConfig::HLSCacheConfig()
        :m3u8_live_cache(NULL),
        ts_live_cache(NULL)
    {
        m3u8_live_cache = new HLSM3U8LiveCacheConfig();
        ts_live_cache = new HLSTSLiveCacheConfig();
    }

    HLSCacheConfig::~HLSCacheConfig()
    {
        if (m3u8_live_cache != NULL)
        {
            delete m3u8_live_cache;
            m3u8_live_cache = NULL;
        }

        if (ts_live_cache != NULL)
        {
            delete ts_live_cache;
            ts_live_cache = NULL;
        }
    }

    int32_t HLSCacheConfig::load_default_config()
    {
        m3u8_live_cache->load_default_config();
        ts_live_cache->load_default_config();

        return 0;
    }

    int32_t HLSCacheConfig::parse(struct xmlnode* input_config)
    {
        struct xmlnode* node = NULL;
        int32_t ret1, ret2;

        if (input_config == NULL)
        {
            WRN("can not find hls_cache config, use default config");
            load_default_config();
            return 0;
        }

        node = xmlgetchild(input_config, "m3u8_live_cache", 0);
        ret1 = m3u8_live_cache->parse(node);

        node = xmlgetchild(input_config, "ts_live_cache", 0);
        ret2 = ts_live_cache->parse(node);

        if (ret1 < 0 || ret2 < 0)
        {
            return -1;
        }

        return 0;
    }

    HLSCacheConfig& HLSCacheConfig::operator
        = (const HLSCacheConfig& right)
    {
        *m3u8_live_cache = *(right.m3u8_live_cache);
        *ts_live_cache = *(right.ts_live_cache);

        return *this;
    }*/

    CacheManagerConfig::CacheManagerConfig()
        :
		//flv_fragment_generator(NULL),
        //ts_segment_generator(NULL),
        flv_block_live_cache(NULL),
        //flv_fragment_cache(NULL),
        //hls_cache(NULL),
        live_push_timeout_sec(10),
        live_req_timeout_sec(120)
	{
        memset(save_stream_dir, 0, sizeof(save_stream_dir));
	}

    CacheManagerConfig::~CacheManagerConfig()
    {
        /*if (flv_fragment_generator != NULL)
        {
            delete flv_fragment_generator;
            flv_fragment_generator = NULL;
        }

        if (ts_segment_generator != NULL)
        {
            delete ts_segment_generator;
            ts_segment_generator = NULL;
        }*/

        if (flv_block_live_cache != NULL)
        {
            delete flv_block_live_cache;
            flv_block_live_cache = NULL;
        }

        /*if (flv_fragment_cache != NULL)
        {
            delete flv_fragment_cache;
            flv_fragment_cache = NULL;
        }

        if (hls_cache != NULL)
        {
            delete hls_cache;
            hls_cache = NULL;
        }*/
    }

    void CacheManagerConfig::init(bool enable_generator )
    {
        //if (enable_generator)
       // {
       //     flv_fragment_generator = new FLVFragmentGeneratorConfig();
       //     ts_segment_generator = new TSSegmentGeneratorConfig();
       // }
        flv_block_live_cache = new FLVBlockLiveCacheConfig();
        //flv_fragment_cache = new FLVFragmentCacheConfig();
        //hls_cache = new HLSCacheConfig();
    }

	int32_t CacheManagerConfig::load_default_config()
	{
        /*if (flv_fragment_generator != NULL)
        {
            flv_fragment_generator->load_default_config();
        }

        if (ts_segment_generator != NULL)
        {
            ts_segment_generator->load_default_config();
        }*/
        //flv_block_live_cache->load_default_config();
        //flv_fragment_cache->load_default_config();
        //hls_cache->load_default_config();

		sprintf(save_stream_dir, "dump_stream");
        live_push_timeout_sec = 10;
        live_req_timeout_sec = 120;

		return 0;
	}

	int32_t CacheManagerConfig::parse(struct xmlnode* input_config)
	{
		struct xmlnode* node = NULL;
        //int32_t ret1 = 0;
        //int32_t ret2 = 0;
        int32_t ret3;
		
		if (input_config == NULL)
		{
			printf("can not find cache_manager config, use default config");
            WRN("can not find cache_manager config, use default config");
			load_default_config();
			return 0;
		}

        /*if (flv_fragment_generator != NULL)
        {
            node = xmlgetchild(input_config, "flv_fragment_generator", 0);
            ret1 = flv_fragment_generator->parse(node);
            if (ret1 < 0)
            {
                printf("can not find flv_fragment_generator config, use default config");
                WRN("can not find flv_fragment_generator config, use default config");
                flv_fragment_generator->load_default_config();
            }
        }

        if (ts_segment_generator != NULL)
        {
            node = xmlgetchild(input_config, "ts_segment_generator", 0);
            ret2 = ts_segment_generator->parse(node);

            if (ret2 < 0)
            {
                printf("can not find ts_segment_generator config, use default config");
                WRN("can not find ts_segment_generator config, use default config");
                ts_segment_generator->load_default_config();
            }
        }*/

		node = xmlgetchild(input_config, "flv_block_live_cache", 0);
        ret3 = flv_block_live_cache->parse(node);

		//node = xmlgetchild(input_config, "flv_fragment_cache", 0);
        //ret4 = flv_fragment_cache->parse(node);

        //node = xmlgetchild(input_config, "hls_cache", 0);
        //ret5 = hls_cache->parse(node);

        if (ret3 < 0 )
            //|| ret4 < 0
            //|| ret5 < 0)
		{
			return -1;
		}

		/* we don't allow change this dir for now.
		char* attr = xmlgetattr(input_config, "save_stream_dir");

		if (!attr)
		{
			ERR("parse cache_manager.save_stream_dir failed.");
			return -1;
		}

		save_stream_dir = attr;
		*/

		char *attr = xmlgetattr(input_config, "live_push_timeout_sec");

		if (!attr)
		{
			ERR("parse cache_manager.live_push_timeout_sec failed.");
			return -1;
		}

        live_push_timeout_sec = atoi(attr);

        attr = xmlgetattr(input_config, "live_req_timeout_sec");

        if (!attr)
        {
            ERR("parse cache_manager.live_req_timeout_sec failed.");
            return -1;
        }

        live_req_timeout_sec = atoi(attr);

		return 0;
	}

	CacheManagerConfig& CacheManagerConfig::operator = (const CacheManagerConfig& right)
	{

        memcpy(save_stream_dir, right.save_stream_dir, strlen(right.save_stream_dir));
        live_push_timeout_sec = right.live_push_timeout_sec;
        live_req_timeout_sec = right.live_req_timeout_sec;

		return *this;
	}

    void CacheManagerConfig::set_default_config()
    {
    }

    bool CacheManagerConfig::load_config(xmlnode* xml_config)
    {
        xml_config = xmlgetchild(xml_config, "common", 0);
        ASSERTR(xml_config != NULL, false);
        xml_config = xmlgetchild(xml_config, "cache_manager", 0);
        ASSERTR(xml_config != NULL, false);

        int32_t ret = parse(xml_config);
        if (ret < 0)
        {
            fprintf(stderr, "cache_manager_config get failed.\n");
            return false;
        }
        else
        {
            return true;
        }
    }

    bool CacheManagerConfig::reload() const
    {
        return (CacheManager::get_cache_manager()->load_config(this) == 0);
    }

    const char* CacheManagerConfig::module_name() const
    {
        return "cache_manager";
    }

    void CacheManagerConfig::dump_config() const
    {
        //#todo
    }

}
