/**
* @file cmd_fsm.h
* @brief cmd finite state machine, manage current connection's state
*
* @author
* @copyright Youku All rights reserved.
* @email
* @company http://www.youku.com
* @date 2015-12-31
*/
#ifndef _CMD_FSM_H_
#define _CMD_FSM_H_
#include <string>
#include "config.h"
#include "proto_define.h"
#include "utils/buffer.hpp"

namespace lcdn
{
namespace net
{
class CMDFSMMgr;
class CMDBaseRoleMgr;
class TCPConnection;
class CMDFSMState;
/**
* @brief cmd finite state machine, manage current connection's state
*/
class CMDFSM
{
    friend class CMDServer;
    friend class CMDFSMMgr;
    friend class CMDFSMStateInit;
public:
    void set_state(CMDFSMState* state);
    CMDFSMState* state();
    /**
    * @brief send buf through conn
    * @param buf content wanted to send
    * @return 0 success -1 error
    */
    int send(lcdn::base::Buffer* buf);
        
    void set_context(void* context);
    void* context();

    uint32_t id();

    void end();
    CMDBaseRoleMgr* get_role_mgr();

    void update();
    TCPConnection* conn();
    time_t last_active_time();
    std::string peerinfo();
    std::string localinfo();
private:
    CMDFSM(CMDFSMMgr* fsm_mgr, TCPConnection* conn, uint32_t id);
    ~CMDFSM();
    uint32_t _handle_read(const proto_header& header, lcdn::base::Buffer* payload);
    uint32_t _handle_write(const proto_header& header, lcdn::base::Buffer* payload);
    CMDFSMMgr* _get_fsm_mgr();
    void _init();
    void _set_role_mgr(CMDBaseRoleMgr* role_mgr);

public:
    lcdn::base::Buffer *out_buffer;
private:
    TCPConnection *_conn;
    CMDFSMMgr *_fsm_mgr;
    CMDFSMState *_state;
    void *_context;
    CMDBaseRoleMgr *_role_mgr;

    uint32_t _fsm_id;
    time_t _last_active_time;
};
} // namespace net
} // namespace lcdn
#endif // _CMD_FSM_H_
