/**
* @file cmd_base_role_mgr.
* @brief 
*
* @author
* @copyright Youku All rights reserved.
* @email
* @company http://www.youku.com
* @date 2015-12-31
*/
#ifndef _CMD_BASE_ROLE_MGR_H_
#define _CMD_BASE_ROLE_MGR_H_
#include <map>
#include "cmd_fsm.h"
#include "utils/buffer.hpp"
#include "proto_define.h"

namespace lcdn
{
namespace net
{
        
/**
* @brief base role mgr
*/
class CMDBaseRoleMgr
{
public:
    virtual ~CMDBaseRoleMgr()
    {
    }
    virtual bool belong(const proto_header& header) = 0;
    /**
    * @brief entry of role mgr
    * @param fsm
    * @param header cmd header
    * @param payload
    * @return concrete role associate with fsm
    */
    virtual void* entry(CMDFSM* fsm, const proto_header& header, lcdn::base::Buffer* payload) = 0;

    virtual void sig_fsm_closed(uint32_t id)
    {
    }
};

} // namespace net
} // namespace lcdn
#endif // _CMD_BASE_ROLE_MGR_H_
