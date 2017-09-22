/**
* @file cmd_server.cpp
* @brief Implementation of the Class CMDServer
*
* @author
* @copyright Youku All rights reserved.
* @email
* @company http://www.youku.com
* @date 2015-12-31
*/
#include <cstdlib>
#include "util/log.h"
#include "event_loop.h" // EventLoop
#include "ip_endpoint.h" // InetAddress
#include "cmd_server.h" // CMDServer
#include "cmd_data_opr.h"
#include "tcp_connection.h" // _map_conns
#include "tcp_server.h" // _tcp_server
#include "cmd_fsm.h"
#include "cmd_fsm_mgr.h" // _fsm_mgr
#include "cmd_base_role_mgr.h"

using namespace std;
using namespace lcdn;
using namespace lcdn::net;

const uint32_t CONN_DELAY= 360;

CMDServer::CMDServer(EventLoop* loop,
    const InetAddress& net_addr,
    const std::string& name)
    : _tcp_server(new TCPServer(loop, net_addr, name, this)),
      _fsm_mgr(new CMDFSMMgr()),
      _default_input_buf_size(DEFAULT_INPUT_BUF_SIZE),
      _default_output_buf_size(DEFAULT_OUTPUT_BUF_SIZE), 
      _loop(loop),
      _conn_delay(CONN_DELAY)
{
    TRC("CMDServer::CMDServer");
    _fsm_timeout_list.clear();
}


CMDServer::~CMDServer()
{
    TRC("CMDServer::~CMDServer");
    if (_tcp_server)
    {
        delete _tcp_server;
        _tcp_server = NULL;
    }
}


void CMDServer::start()
{
    TRC("CMDServer::start");
    // need a interface to confirm whether TCPServer is started or not
    if (NULL != _tcp_server)
    {
        // init TCPServer, and start
        _tcp_server->start();
    }       
}


void CMDServer::on_close(TCPConnection* conn)
{
    CMDFSM* fsm = NULL;

    if (NULL == conn)
    {
        WRN("CMDServer::on_close: conn is null!");
        return;
    }

    INF("CMDServer::on_close: conn_id: %d", conn->id());

    fsm = static_cast<CMDFSM*>(conn->context()); 
    if (NULL != fsm)
    {
        if (NULL != _fsm_mgr)
        {
            fsm->end();
            _fsm_mgr->remove_cmd_fsm(fsm);
            _fsm_timeout_list.remove(fsm);
        }

        conn->set_context(NULL);
    }
}


void CMDServer::on_connect(TCPConnection* conn)
{
    TRC("CMDServer::on_connect");
    if (NULL != conn)
    {
        CMDFSM* fsm = NULL;

        conn->set_input_buffer_size(_default_input_buf_size);
        conn->set_output_buffer_size(_default_output_buf_size);

        // conn hasn't bound a fsm
        // create fsm based on conn, insert to _fsm_mgr, and return fsm
        if (NULL != _fsm_mgr)
        {
            if (NULL == conn->context())
            {
                fsm = _fsm_mgr->create_cmd_fsm(conn);
                conn->set_context(fsm);
                fsm->update();
                _fsm_timeout_list.push_back(fsm);

                INF("CMDServer::on_connect: _fsm_timeout_list push_back fsm %d, peerinfo %s", fsm->id(), fsm->peerinfo().c_str());
            }
            else
            {
                // #TODO
                // conn already has a fsm, an unexpected error occured; 
                if (NULL != _tcp_server)
                {
                    INF("CMDServer::on_connect: conn already has a fsm!");
                    _tcp_server->close(conn->id());
                }
            }
        }
    }
}


void CMDServer::on_read(TCPConnection* conn)
{
    if (NULL == conn)
    {
        WRN("CMDServer::on_read: conn is null!");
        return;
    }

    CMDDataOpr::FrameStates frame_state = CMDDataOpr::INVALID_DATA;
    proto_header header;
    uint32_t header_size = sizeof(proto_header);

    CMDFSM* fsm = static_cast<CMDFSM*>(conn->context());

    if (NULL != fsm)
    {
        // update last_active_time,_fsm_timeout_list
        fsm->update();
        _fsm_timeout_list.remove(fsm);
        _fsm_timeout_list.push_back(fsm);

        INF("CMDServer::on_read: _fsm_timeout_list len: %d, fsm id: %d, \
                conn id: %d, peerinfo %s, fsm last_update_time %d", \
                _fsm_timeout_list.size(), fsm->id(), conn->id(), \
                fsm->peerinfo().c_str(), fsm->last_active_time());

        lcdn::base::Buffer* buf = conn->input_buffer();
        frame_state = CMDDataOpr::is_frame(buf, header);

        while ((frame_state == CMDDataOpr::VALID_DATA))
        {
            // hand over to fsm to handle read
            buf->eat(header_size);

            fsm->_handle_read(header, buf);

            buf->eat(header.size-header_size);
            
            frame_state = CMDDataOpr::is_frame(buf, header);
        }

        // invalid frame, close conn
        if (frame_state == CMDDataOpr::INVALID_DATA)
        {
            if (NULL != _tcp_server)
            {
                INF("CMDServer::on_read: frame invalid!, close connection. conn id: %d", conn->id());
                _tcp_server->close(conn->id());
            }
        }

        buf->adjust();
    }
    else
    {
        if (NULL != _tcp_server)
        {
            INF("CMDServer::on_read:fsm is NULL, close connection. conn id: %d", conn->id());
            _tcp_server->close(conn->id());
        }
    }
}


void CMDServer::on_write_completed(TCPConnection* conn)
{
    TRC("CMDServer::on_write_completed");
    if (NULL == conn)
    {
        WRN("CMDServer::on_write_completed: conn is null!");
        return;
    }

    CMDFSM* fsm = static_cast<CMDFSM*>(conn->context());

    if (NULL != fsm)
    {
        // update last_active_time,_fsm_timeout_list
        fsm->update();
        _fsm_timeout_list.remove(fsm);
        _fsm_timeout_list.push_back(fsm);
    }
}


void CMDServer::on_error(TCPConnection* conn)
{
}


void CMDServer::on_second(time_t t)
{
    TRC("CMDServer::on_second");
    std::list<CMDFSM*>::iterator iter = _fsm_timeout_list.begin();

    for ( ; iter != _fsm_timeout_list.end(); )
    {
        std::list<CMDFSM*>::iterator tmp_iter = iter++;
        CMDFSM* fsm = *tmp_iter;
        if (NULL != fsm)
        {
            time_t last_active_time = fsm->last_active_time();

            if (t - last_active_time >= _conn_delay)
            {
                // timeout, close connection
                if (NULL != _tcp_server)
                {
                    TCPConnection* conn = fsm->conn();
                    if (NULL != conn)
                    {
                        DBG("CMDServer::on_second:conn %d is unactive!", conn->id());
                        _tcp_server->close(conn->id());
                    }
                }
            }
            else
            {
                break;
            }
        }
    }
}


void CMDServer::register_role_mgr(CMDBaseRoleMgr* role_mgr)
{
    if (NULL == role_mgr)
    {
        WRN("CMDServer::register_role_mgr:role_mgr is null!");
        return;
    }

    if (NULL != _fsm_mgr)
    {
        _fsm_mgr->register_role_mgr(role_mgr);
    }
}


void CMDServer::remove_role_mgr(CMDBaseRoleMgr* role_mgr)
{
    if (NULL == role_mgr)
    {
        WRN("CMDServer::remove_role_mgr:role_mgr is null!");
        return;
    }

    if (NULL != _fsm_mgr)
    {
        _fsm_mgr->remove_role_mgr(role_mgr);
    }
}


void CMDServer::remove_all_role_mgrs()
{
    if (NULL != _fsm_mgr)
    {
        _fsm_mgr->remove_all_role_mgrs();
    }
}


list<CMDBaseRoleMgr*> CMDServer::role_mgrs()
{
    list<CMDBaseRoleMgr*> role_mgrs;

    role_mgrs.clear();

    if (NULL != _fsm_mgr)
    {
        role_mgrs = _fsm_mgr->role_mgrs();
    }

    return role_mgrs;
}

CMDFSM* CMDServer::find_cmd_fsm(int fsm_id)
{
    CMDFSM* fsm = NULL;

    if (NULL != _fsm_mgr)
    {
        fsm = _fsm_mgr->find_cmd_fsm(fsm_id);
    }
    return fsm;
}


CMDFSMMgr* CMDServer::get_fsm_mgr()
{
    return _fsm_mgr;
}
