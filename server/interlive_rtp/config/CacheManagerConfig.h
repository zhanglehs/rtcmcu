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
#include "config/ConfigManager.h"

namespace media_manager {

  class FragmentGeneratorConfig {
  public:
    FragmentGeneratorConfig();
    virtual ~FragmentGeneratorConfig();
    FragmentGeneratorConfig& operator=(const FragmentGeneratorConfig& right);

    int32_t delay_time_msec;
  };

  class FLVBlockLiveCacheConfig : public FragmentGeneratorConfig {
  public:
    FLVBlockLiveCacheConfig();
    ~FLVBlockLiveCacheConfig();
    FLVBlockLiveCacheConfig& operator=(const FLVBlockLiveCacheConfig& right);

    int32_t load_default_config();
    int32_t parse(struct xmlnode* input_config);
    int32_t buffer_duration_sec;
  };

  class CacheManagerConfig : public ConfigModule {
  public:
    CacheManagerConfig();
    virtual ~CacheManagerConfig();
    CacheManagerConfig& operator=(const CacheManagerConfig& right);

    int32_t load_default_config();
    int32_t parse(struct xmlnode* input_config);

  public:
    virtual void set_default_config();
    virtual bool load_config(xmlnode* xml_config);
    virtual bool reload() const;
    virtual const char* module_name() const;
    virtual void dump_config() const;

    FLVBlockLiveCacheConfig* flv_block_live_cache;
    char save_stream_dir[4096];
    int32_t live_push_timeout_sec;
    int32_t live_req_timeout_sec;
  };
};

#endif  /* __CACHE_MANAGER_CONFIG_H_ */
