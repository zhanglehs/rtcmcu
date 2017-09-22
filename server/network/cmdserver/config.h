#ifndef _CONFIG_H_
#define _CONFIG_H_
#include "proto_define.h"

namespace lcdn
{
namespace net
{
/**
* @brief role info of fsm's manager
*/
class RoleInfo
{
public:
    uint32_t ip;
    uint16_t asn;
    uint16_t region;
};

typedef enum
{
    PLAYER_ROLE=10001,
    UPLOADER_ROLE,
    FORWARD_CLINET_ROLE,
    FORWARD_SERVER_ROLE
} CMDFSMRole;

}// namespace net
}// namespace lcdn
#endif
