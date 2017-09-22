/*
 *
 * author: hechao@youku.com
 * create: 2014/4/10
 *
 */

#ifndef __XM_ANT_H_
#define __XM_ANT_H_

#include <string>
#include <utils/buffer_queue.h>
#include <xthread/thread.hpp>
#include "global_var.h"
#include "streamid.h"

class AntConfig
{
    public:    
        AntConfig(){}
        virtual ~AntConfig(){}
};

class AdminServer;

class Ant: public Thread
{
    public:
        Ant(std::vector<StreamId_Ext> stream_id, AdminServer *admin)
            :_stream_id(stream_id), _admin(admin), _force_exit(false)
        {}
        virtual ~Ant()
        {
            LOG_DEBUG(g_logger, "~Ant");
        }

        /*
        int set_rtmp_config(std::string rtmp_svr_ip,
                uint16_t rtmp_svr_port,
                std::string rtmp_app_name,
                std::string rtmp_stream_name);
        */
        void set_buffer(lcdn::base::BufferQueue * queue) { _buffer = queue;}

        virtual void stop() = 0;

        virtual int force_exit()
        {
            LOG_INFO(g_logger, "Ant: force_exit, stream_id=" << _stream_id[0].unparse());
            this->stop();

            return 0;
        }

    protected:
        void _stop(bool restart);
        int _push_data(const char *buf, size_t size, int stream_index);

    protected:
        std::vector<StreamId_Ext> _stream_id;
        lcdn::base::BufferQueue *_buffer;
        AdminServer *_admin;

    private:
        bool _force_exit;
};

#endif

