/**
* @file cmd_fsm_mgr.h
* @brief manager of cmd_fsm, manage all cmd_fsms
*
* @author
* @copyright Youku All rights reserved.
* @email
* @company http://www.youku.com
* @date 2015-12-31
*/
#ifndef _CMD_FSM_MGR_H_
#define _CMD_FSM_MGR_H_
#include <list>
#include <map>
#include "proto_define.h"
#include "cmd_fsm.h"
#include "utils/buffer.hpp"
#include "cmd_base_role_mgr.h"

namespace lcdn
{
namespace net
{
typedef std::map<int, CMDFSM*> cmd_fsm_map_t;
typedef std::list<CMDBaseRoleMgr*> cmd_role_mgr_list_t;

class TCPServer;
/**
* @brief cmd_fsm's manager,to dispatch the invoke
*/
class CMDFSMMgr
{
public:
    CMDFSMMgr();
    virtual ~CMDFSMMgr();

   /**
    * @brief create CMDFSM by TCPConnection 
    * @param conn TCPConnection
    * @return cmd_fsm
    */
    CMDFSM* create_cmd_fsm(TCPConnection* conn);
    void remove_cmd_fsm(CMDFSM* fsm);
    /**
    * @brief obtain CMDFSM by fsm id
    * @param fsm_id fsm id
    * @return
    */
    CMDFSM* find_cmd_fsm(int fsm_id);

    void register_role_mgr(CMDBaseRoleMgr* role_mgr);
    void remove_role_mgr(CMDBaseRoleMgr* role_mgr);
    void remove_all_role_mgrs();

    cmd_role_mgr_list_t role_mgrs();
 /**
    * @brief find who can manipulate buf
    * @param header cmd header
    * @return role_mgr who can manipulate buf
    */
    CMDBaseRoleMgr* belong_to_role_mgr(const proto_header& header);

    CMDFSMState* get_fsm_state_init();
    CMDFSMState* get_fsm_state_end();
private:
    cmd_role_mgr_list_t _role_mgr_list;
    cmd_fsm_map_t _fsm_map;
    CMDFSMState* _fsm_state_init;
    CMDFSMState* _fsm_state_end;
    uint32_t _last_id;
};
} // namespace net
} // namespace lcdn
#endif // _CMD_FSM_MGR_H_
