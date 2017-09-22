/**
* @file cmd_fsm_state_init.h
* @brief 
*
* @author
* @copyright Youku All rights reserved.
* @email
* @company http://www.youku.com
* @date 2015-12-31
*/
#ifndef _CMD_FSM_STATE_INIT_H_
#define _CMD_FSM_STATE_INIT_H_

#include "cmd_fsm_state.h"

namespace lcdn
{
namespace net
{

class CMDFSMStateInit : public CMDFSMState
{
public:
    CMDFSMStateInit()
    {
    }

    virtual ~CMDFSMStateInit()
    {
    }

    virtual int handle_read(CMDFSM* fsm, const proto_header& header, lcdn::base::Buffer* payload);
    virtual int handle_write(CMDFSM* fsm, const proto_header& header, lcdn::base::Buffer* payload);

};

}// net
}// lcdn
#endif // _CMD_FSM_STATE_INIT_H_
