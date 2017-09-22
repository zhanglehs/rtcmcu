/**
* @file cmd_fsm_state_end.cpp
* @brief
*
* @author
* @copyright Youku All rights reserved.
* @email
* @company http://www.youku.com
* @date 2015-12-31
*/
#include "cmd_fsm.h"
#include "cmd_fsm_state.h"
#include "cmd_fsm_state_end.h"

using namespace std;
using namespace lcdn;
using namespace lcdn::net;
using namespace lcdn::base;
int CMDFSMStateEnd::handle_read(CMDFSM* fsm, const proto_header& header, Buffer* payload)
{
    return 0;
}


int CMDFSMStateEnd::handle_write(CMDFSM* fsm, const proto_header& header, Buffer* payload)
{

    return 0;
}
