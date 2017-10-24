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

#include "config/CacheManagerConfig.h"
#include "media_manager/cache_manager.h"

#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <iostream>

#include "../util/log.h"
#include "../util/xml.h"
//#include "cache_manager_config.h"
//#include "cache_manager.h"

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

  //////////////////////////////////////////////////////////////////////////

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

  //////////////////////////////////////////////////////////////////////////

  CacheManagerConfig::CacheManagerConfig()
    :live_push_timeout_sec(10), live_req_timeout_sec(120) {
    memset(save_stream_dir, 0, sizeof(save_stream_dir));
    flv_block_live_cache = new FLVBlockLiveCacheConfig();
  }

  CacheManagerConfig::~CacheManagerConfig()
  {
    delete flv_block_live_cache;
    flv_block_live_cache = NULL;
  }

  int32_t CacheManagerConfig::load_default_config()
  {
    sprintf(save_stream_dir, "dump_stream");
    live_push_timeout_sec = 10;
    live_req_timeout_sec = 120;

    return 0;
  }

  int32_t CacheManagerConfig::parse(struct xmlnode* input_config)
  {
    struct xmlnode* node = NULL;
    int32_t ret3;

    if (input_config == NULL)
    {
      printf("can not find cache_manager config, use default config");
      WRN("can not find cache_manager config, use default config");
      load_default_config();
      return 0;
    }

    node = xmlgetchild(input_config, "flv_block_live_cache", 0);
    ret3 = flv_block_live_cache->parse(node);

    if (ret3 < 0)
    {
      return -1;
    }

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
    return (FlvCacheManager::Instance()->load_config(this) == 0);
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
