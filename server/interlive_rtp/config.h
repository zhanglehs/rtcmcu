#ifndef __CONFIG_TMP_H
#define __CONFIG_TMP_H

#include "player/module_player.h"
#include "player/rtp_player_config.h"
#include "uploader/uploader_config.h"
#include "uploader/rtp_uploader_config.h"
#include "backend_new/module_backend.h"
#include "backend_new/rtp_backend_config.h"
#include "backend_new/forward_client_rtp_tcp.h"
//#include "module_tracker.h"
#include "media_manager/cache_manager_config.h"
//#include "http_server.h"
#include "network/base_http_server.h"
#include "target_config.h"
#include "publisher/flv_publisher.h"

struct config
{
    TargetConfig    target_conf;
    player_config   player;
    RTPPlayerConfig rtp_player_config;
//#ifdef MODULE_UPLOADER
    uploader_config uploader;
    RTPUploaderConfig rtp_uploader_config;
//#endif
    backend_config      backend;
    RTPBackendConfig    rtp_backend_config;
    //ModTrackerConfig    tracker;
    media_manager::CacheManagerConfig cache_manager_config;
    //http_server_config  http_config;
    FCRTPConfig         fcrtp_config;
    http::HTTPServerConfig http_ser_config;
	ModPublisherConfig  publisher_config;

    config& operator=(const config& rhv)
    {
        target_conf         = rhv.target_conf;
        player              = rhv.player;
        rtp_player_config   = rhv.rtp_player_config;
//#ifdef MODULE_UPLOADER
        uploader            = rhv.uploader;
        rtp_uploader_config = rhv.rtp_uploader_config;
//#endif
        backend             = rhv.backend;
        rtp_backend_config  = rhv.rtp_backend_config;
        //tracker             = rhv.tracker;
        cache_manager_config= rhv.cache_manager_config;
        //http_config         = rhv.http_config;
        fcrtp_config        = rhv.fcrtp_config;
        http_ser_config     = rhv.http_ser_config;
		publisher_config    = rhv.publisher_config;
        return *this;
    }
};

#endif

