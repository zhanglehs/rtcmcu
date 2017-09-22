#ifndef _TEST_FSM_MGR_H_
#define _TEST_FSM_MGR_H_
#include "cmd_fsm.h"
#include "cmd_fsm_state.h"
#include "cmd_base_role_mgr.h"
#include <assert.h>


using namespace lcdn;
using namespace lcdn::net;

/**
* @brief player fsm mgr
*/
class Test_Player_FSM_Mgr : public CMDBaseRoleMgr
{
public:
    explicit Test_Player_FSM_Mgr()
    {
    }
    virtual bool belong(const proto_header& header)
    {

        return true;
    }

    virtual void* entry(CMDFSM* fsm, const proto_header& header, Buffer* payload)
    {
        return NULL;
    }
};

/**
* @brief specific fsm_mgr's delegator
*/
class Test_Uploader_FSM_Mgr : public CMD_Base_FSM_Mgr
{
public:
    explicit Test_Uploader_FSM_Mgr()
    {
    }

    virtual bool belong(const proto_header& header)
    {

        return true;
    }

    virtual void* entry(CMDFSM* fsm, const proto_header& header, Buffer* payload)
    {
        return NULL;
    }
};
#endif
