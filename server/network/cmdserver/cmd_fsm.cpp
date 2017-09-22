/**
* @file cmd_fsm.cpp
* @brief 
*
* @author
* @copyright Youku All rights reserved.
* @email
* @company http://www.youku.com
* @date 2015-12-31
*/
#
#include <string.h>
#include <time.h>
#include "proto_define.h"
#include "tcp_connection.h"
#include "cmd_fsm_state.h"
#include "cmd_fsm.h"
#include "cmd_fsm_mgr.h"
#include "cmd_data_opr.h"
#include "cmd_base_role_mgr.h"
#include "util/log.h"
#include "net_errors.h"

using namespace lcdn;
using namespace lcdn::net;
using namespace lcdn::base;

const uint32_t FSM_BUF_SIZE = 20*1024*1024;
/**
 * cmd_fsm's constructor
 */
CMDFSM::CMDFSM(CMDFSMMgr* fsm_mgr, TCPConnection* conn, uint32_t id)
    : _conn(conn),
      _fsm_mgr(fsm_mgr),
      _state(NULL),
      _context(NULL),
      _role_mgr(NULL),
      _fsm_id(id),
      _last_active_time(0)
{
    out_buffer = new (std::nothrow) Buffer(FSM_BUF_SIZE);
    if (NULL == out_buffer)
    {
        ERR("CMDFSM::CMDFSM: new error!");
    }
}


CMDFSM::~CMDFSM()
{
    _conn = NULL;
    _fsm_mgr = NULL;
    _state = NULL;
    _context = NULL;
    _role_mgr = NULL;

    if (NULL != out_buffer)
    {
        delete out_buffer;
        out_buffer = NULL;
    }
}


/**
 * handle read callback
 */
uint32_t CMDFSM::_handle_read(const proto_header& header, Buffer* payload)
{
    // fsm has not yet associate with concrete application
    if (_context == NULL)
    {
        CMDBaseRoleMgr* role_mgr = NULL;

        role_mgr = _fsm_mgr->belong_to_role_mgr(header);
        if (NULL != role_mgr)
        {
            void* role = role_mgr->entry(this, header, payload);
            set_context(role);
            _set_role_mgr(role_mgr);
        }
    }

    if (NULL != _state)
    {
        _state->handle_read(this, header, payload);
    }

    return 0;
}


/**
 * handle write callback
 */
uint32_t CMDFSM::_handle_write(const proto_header& header, Buffer* payload)
{

    return 0;
}

void CMDFSM::set_state(CMDFSMState* state)
{
    _state = state;
}


CMDFSMState* CMDFSM::state()
{
    return _state;
}


int CMDFSM::send(lcdn::base::Buffer* buf)
{
    NetError rtn;

    if (NULL != _conn)
    {
       rtn = (NetError)_conn->send(buf);
    }

    if (rtn == OK)
    {
        return 0;
    }
    else if(rtn == ERR_BUFFER_SIZE_LOW)
    {
        return -1;
    }
    else
    {
        // ERR_CONNECTION_CLOSED
        return -2;
    }
}


void CMDFSM::set_context(void* context)
{
    _context = context;
}


void* CMDFSM::context()
{
    return _context;
}


uint32_t CMDFSM::id()
{
    return _fsm_id;
}


CMDFSMMgr* CMDFSM::_get_fsm_mgr()
{
    return _fsm_mgr;
}

void CMDFSM::_init()
{
    if (NULL != _fsm_mgr)
    {
        set_state(_fsm_mgr->get_fsm_state_init());
    }
}

void CMDFSM::end()
{
    TRC("CMDFSM::end");
    if (NULL != _role_mgr)
    {
        _role_mgr->sig_fsm_closed(_fsm_id);
    }
}


void CMDFSM::update()
{
   _last_active_time = time(NULL); 
}


TCPConnection* CMDFSM::conn()
{
    return _conn;
}


std::string CMDFSM::peerinfo()
{
    std::string str;

    if (NULL != _conn)
    {
        str = _conn->peer_address().to_host_port();
    }

    return str; 
}

std::string CMDFSM::localinfo()
{
    std::string str;

    if (NULL != _conn)
    {
        str = _conn->local_address().to_host_port();
    }

    return str; 
}

time_t CMDFSM::last_active_time()
{
    return _last_active_time;
}


void CMDFSM::_set_role_mgr(CMDBaseRoleMgr* role_mgr)
{
    _role_mgr = role_mgr;
}


CMDBaseRoleMgr* CMDFSM::get_role_mgr()
{
    return _role_mgr;
}
