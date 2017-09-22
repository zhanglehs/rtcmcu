/**
* @file cmd_data_opr.h
* @brief :
* @author
* @copyright Youku All rights reserved.
* @email
* @company http://www.youku.com
* @date 2015-12-31
*/
#ifndef _CMD_DATA_OPR_H_
#define _CMD_DATA_OPR_H_

#include "proto_define.h"
#include "utils/buffer.hpp"

namespace lcdn
{
namespace net
{
class CMDDataOpr
{
public:
    enum FrameStates { VALID_DATA, INVALID_DATA, INCOMPLETE_DATA };

    /*
     * get proto_header from buf, and eat sizeof(proto_header) from buf
     */
    static FrameStates is_frame(lcdn::base::Buffer* inbuf, proto_header& header);

private:
    static FrameStates _is_valid(proto_header &header);
};
}// namespace net
}// namespace lcdn


#endif // _CMD_DATA_OPR_H_
