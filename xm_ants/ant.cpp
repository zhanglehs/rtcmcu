/*
 *
 * author: hechao@youku.com
 * create: 2014/4/10
 *
 */
#include "ant.h"
#include "admin_server.h"
 
void Ant::_stop(bool restart)
{
    if (_admin)
    {
        _admin->ant_callback(_stream_id[0], ANT_EVENT_STOPED, restart);
    }

    LOG_INFO(g_logger, "an ant task finish");
}

int Ant::_push_data(const char *buf, size_t size, int stream_index)
{
    int ret = _buffer[stream_index].push_data(buf, size);
    LOG_DEBUG(g_logger, "_buffer push " << ret << " data to cache between ant and sender.");
    return ret > 0 ? ret : 0;
    
}

