/**
* @file cmd_fsm_state.h
* @brief 
*
* @author
* @copyright Youku All rights reserved.
* @email
* @company http://www.youku.com
* @date 2015-12-31
*/
#ifndef _CMD_FSM_STATE_H_
#define _CMD_FSM_STATE_H_
#include "proto_define.h"
#include "utils/buffer.hpp"
#include "event_loop.h"

namespace lcdn
{
namespace net
{

class CMDFSM;
class CMDFSMState
{
public:
    CMDFSMState()
    {
    }

    virtual ~CMDFSMState()
    {
    }

    virtual int handle_read(CMDFSM* fsm, const proto_header& header, lcdn::base::Buffer* payload) = 0;
    virtual int handle_write(CMDFSM* fsm, const proto_header& header, lcdn::base::Buffer* payload) = 0;

};

} // namespace net
} // namespace lcdn
#endif // _CMD_FSM_STATE_H_
