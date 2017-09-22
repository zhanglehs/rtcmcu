/**
* @file cmd_fsm_mgr.cpp
* @brief
*
* @author
* @copyright Youku All rights reserved.
* @email
* @company http://www.youku.com
* @date 2015-12-31
*/
#include <list>
#include <algorithm>
#include "util/log.h"
#include "tcp_server.h"
#include "tcp_connection.h"
#include "cmd_fsm.h"
#include "cmd_fsm_state.h"
#include "cmd_fsm_state_init.h"
#include "cmd_fsm_state_end.h"
#include "cmd_base_role_mgr.h"
#include "cmd_fsm_mgr.h"
#include "cmd_data_opr.h"
using namespace std;
using namespace lcdn;
using namespace lcdn::net;

CMDFSMMgr::CMDFSMMgr()
    : _fsm_state_init(NULL),
      _fsm_state_end(NULL),
      _last_id(0)
{
}

CMDFSMMgr::~CMDFSMMgr()
{
    // release all fsm
    cmd_fsm_map_t::iterator iter;

    for (iter = _fsm_map.begin(); iter != _fsm_map.end(); iter++)
    {
        CMDFSM* fsm = iter->second;
        if (NULL != fsm)
        {
            delete fsm;
            fsm = NULL;
        }
    }

    _fsm_map.clear();
    _role_mgr_list.clear();

    if (NULL != _fsm_state_init)
    {
        delete _fsm_state_init;
        _fsm_state_init = NULL;
    }

    if (NULL != _fsm_state_end)
    {
        delete _fsm_state_end;
        _fsm_state_end = NULL;
    }
}


CMDFSM* CMDFSMMgr::create_cmd_fsm(TCPConnection* conn)
{
    CMDFSM* fsm = NULL;
    uint32_t id = _last_id++; 
    
    // create fsm
    fsm = new (std::nothrow) CMDFSM(this, conn, id);

    if (NULL == fsm)
    {
        WRN("CMDFSMMgr::create_cmd_fsm: fsm is null!");
        return NULL;
    }

    // set init state
    fsm->_init();
    
    // insert to _fsm_map
    _fsm_map.insert(cmd_fsm_map_t::value_type(id, fsm));

    return fsm;
}


void CMDFSMMgr::remove_cmd_fsm(CMDFSM* fsm)
{
    if ( NULL == fsm)
    {
        WRN("CMDFSMMgr::remove_cmd_fsm: fsm is null!");
        return;
    }

    uint32_t id = fsm->id();
    cmd_fsm_map_t::iterator iter = _fsm_map.find(id);
    if (iter != _fsm_map.end())
    {
        _fsm_map.erase(iter);
    }

    delete fsm;
    fsm = NULL;
}


CMDFSM* CMDFSMMgr::find_cmd_fsm(int fsm_id)
{
    CMDFSM* fsm = NULL;

    cmd_fsm_map_t::iterator iter = _fsm_map.find(fsm_id);
    if (iter != _fsm_map.end())
    {
        // obtain fsm named fsm_id
        fsm = iter->second;
    }

    return  fsm;
}


void CMDFSMMgr::register_role_mgr(CMDBaseRoleMgr* role_mgr)
{
    if ( NULL == role_mgr )
    {
        WRN("CMDFSMMgr::register_role_mgr: role_mgr is null!");
        return;
    }

    // find _fsm_mgr_map by role_type 
    cmd_role_mgr_list_t::iterator iter = find(_role_mgr_list.begin(), _role_mgr_list.end(), role_mgr);

    if (iter == _role_mgr_list.end())
    {
        _role_mgr_list.push_back(role_mgr);
    }

    return;

}


void CMDFSMMgr::remove_role_mgr(CMDBaseRoleMgr* role_mgr)
{
    if ( NULL == role_mgr)
    {
        WRN("CMDFSMMgr::remove_role_mgr: role_mgr is null!");
        return;
    }
    
    // find _fsm_mgr_map by role_type 
    cmd_role_mgr_list_t::iterator iter = find(_role_mgr_list.begin(), _role_mgr_list.end(), role_mgr);

    if (iter != _role_mgr_list.end())
    {
        _role_mgr_list.remove(role_mgr);
    }

    return;

}


void CMDFSMMgr::remove_all_role_mgrs()
{
    _role_mgr_list.clear();

    return;
}


cmd_role_mgr_list_t CMDFSMMgr::role_mgrs()
{
    return _role_mgr_list;
}


CMDBaseRoleMgr* CMDFSMMgr::belong_to_role_mgr(const proto_header& header)
{
    CMDBaseRoleMgr* role_mgr = NULL;
    cmd_role_mgr_list_t::iterator iter; 

    for (iter = _role_mgr_list.begin(); iter != _role_mgr_list.end(); iter++)
    {
        if (((CMDBaseRoleMgr*)*iter)->belong(header))
        {
            role_mgr = *iter;
            break;
        }
    }

    return role_mgr;
}


CMDFSMState* CMDFSMMgr::get_fsm_state_init()
{
    if (NULL == _fsm_state_init)
    {
        _fsm_state_init = new CMDFSMStateInit();
    }

    return _fsm_state_init;
}


CMDFSMState* CMDFSMMgr::get_fsm_state_end()
{
    if (NULL == _fsm_state_end)
    {
        _fsm_state_end = new CMDFSMStateEnd();
    }

    return _fsm_state_end;
}
