/*
 *
 * author: hechao@youku.com
 * create: 2014/3/12
 *
 */

#ifndef __RTMP_ANT_H_
#define __RTMP_ANT_H_

#include <string>
#include <xthread/thread.hpp>
#include <utils/buffer_queue.h>
#include "ant.h"
#include "global_var.h"




#include "stdio.h"
#include <librtmp/rtmp.h>



class AdminServer;

class RTMPAnt:public Ant
{
    public:
        RTMPAnt(std::vector<StreamId_Ext> stream_id, AdminServer *admin)
        :Ant(stream_id, admin),
        _trigger_off(false)
        {}
        virtual ~RTMPAnt() { LOG_DEBUG(g_logger, "~RTMPAnt");}

        int set_rtmp_config(std::string rtmp_svr_ip,
                uint16_t rtmp_svr_port,
                std::string rtmp_app_name,
                std::string rtmp_stream_name);
        int set_rtmp_uri(std::string rtmp_uri);

        void run1();
        virtual void run();
        virtual void stop();

    private:
        int _main(int argc, char **argv);
        int _download(RTMP * rtmp,      // connected RTMP object
                uint32_t dSeek, uint32_t dStopOffset,
                double duration, int bResume, char *metaHeader,
                uint32_t nMetaHeaderSize, char *initialFrame,
                int initialFrameType, uint32_t nInitialFrameSize,
                int nSkipKeyFrames, int bStdoutMode, int bLiveStream,
                int bHashes, int bOverrideBufferTime, uint32_t bufferTime, double *percent);    // percentage downloaded [out]

        int _reconnect_server(RTMP* rtmp, int bufferTime, int dSeek,uint32_t timeout);
        int _is_exist_str(char* buffer, int bufferlen, const char* substr, int substrlen);

    private:
        std::string _rtmp_uri;

        std::string _rtmp_svr_ip;
        uint16_t _rtmp_svr_port;
        std::string _rtmp_app_name;
        std::string _rtmp_stream_name;

        bool _trigger_off;
};


#endif

