#ifndef __CONFIG_TMP_H
#define __CONFIG_TMP_H

#include "connection_manager/FlvConnectionManager.h"
#include "connection_manager/rtp_player_config.h"
#include "connection_manager/rtp_uploader_config.h"
#include "relay/rtp_backend_config.h"
#include "media_manager/cache_manager_config.h"
#include "network/base_http_server.h"
#include "target_config.h"
#include "relay/flv_publisher.h"

struct config
{
  TargetConfig target_conf;
  FlvPlayerConfig flv_player_config;
  RTPPlayerConfig rtp_player_config;
  RTPUploaderConfig rtp_uploader_config;
  RtpRelayConfig  rtp_relay_config;
  media_manager::CacheManagerConfig cache_manager_config;
  HTTPServerConfig http_ser_config;
  ModPublisherConfig  publisher_config;

  config& operator=(const config& rhv)
  {
    target_conf = rhv.target_conf;
    flv_player_config = rhv.flv_player_config;
    rtp_player_config = rhv.rtp_player_config;
    rtp_uploader_config = rhv.rtp_uploader_config;
    rtp_relay_config = rhv.rtp_relay_config;
    cache_manager_config = rhv.cache_manager_config;
    http_ser_config = rhv.http_ser_config;
    publisher_config = rhv.publisher_config;
    return *this;
  }
};

#endif
