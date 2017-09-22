
#include "cmd_data_opr.h"
#include <netinet/in.h>
#include <log.h>

using namespace lcdn;
using namespace lcdn::net;
using namespace lcdn::base;

CMDDataOpr::FrameStates CMDDataOpr::is_frame(Buffer *inbuf, proto_header &header)
{
    if (NULL == inbuf)
    {
        return INVALID_DATA;
    }

    if (inbuf->data_len() < sizeof(proto_header))
    {
        return INCOMPLETE_DATA; 
    }
    
    proto_header *ih = (proto_header*)(inbuf->data_ptr());
    /*switch (ih->version)
    {
    case VER_1:
        break;

    case VER_2:
        if (ih->magic != MAGIC_2)
            return -1;
        break;

    case VER_3:
        if (ih->magic != MAGIC_3)
            return -1;
        break;

    default:
        return -1;
    }
    */

    header.magic = ih->magic;
    header.version = ih->version;
    header.cmd = ntohs(ih->cmd);
    header.size = ntohl(ih->size);

    if (header.size > inbuf->data_len())
    {
        return INCOMPLETE_DATA;
    }

    return _is_valid(header);
}


CMDDataOpr::FrameStates CMDDataOpr::_is_valid(proto_header &header)
{
    uint16_t cmd = header.cmd;
    FrameStates rtn;

    switch (cmd)
    {
        // forward <----> forward v1
        // used in forward_server and forward_client.
        // 1~24
        case CMD_FC2FS_REQ_STREAM:
        case CMD_FS2FC_RSP_STREAM:
        case CMD_FS2FC_STREAMING_HEADER:
        case CMD_FS2FC_STREAMING:
        case CMD_FC2FS_KEEPALIVE:
        case CMD_FC2FS_UNREQ_STREAM:

        // forward <----> forward v2
        // used in forward_server_v2 and forward_client_v2, but only support short stream id,
        // already replaced by f2f v3.
        case CMD_FC2FS_START_TASK:
        case CMD_FS2FC_START_TASK_RSP:
        case CMD_FS2FC_STREAM_DATA:
        case CMD_F2F_STOP_TASK:
        // case CMD_F2F_STOP_TASK_RSP:

        // forward <----> forward v3
        // still used in forward_server_v2 and forward_clinet_v2, support long stream id.
        // 25~49
        case CMD_FC2FS_START_TASK_V3:
        case CMD_F2F_STOP_TASK_V3:
        case CMD_FS2FC_START_TASK_RSP_V3:
        case CMD_FS2FC_STREAM_DATA_V3:

        // forward <----> portal
        // 50~99
        case CMD_F2P_KEEPALIVE:
        case CMD_P2F_INF_STREAM:
        case CMD_P2F_START_STREAM:
        case CMD_P2F_CLOSE_STREAM:
        case CMD_P2F_INF_STREAM_V2:
        case CMD_P2F_START_STREAM_V2:
        case CMD_F2P_WHITE_LIST_REQ:

        // receiver <----> portal
        // 100~149
        case CMD_R2P_KEEPALIVE:

        // uploader<--->up_sche
        // 150~200
        case CMD_U2US_REQ_ADDR:
        case CMD_US2U_RSP_ADDR:

        // tracker<--->forward
        // 200~249
        case CMD_F2T_REGISTER_REQ:
        case CMD_F2T_REGISTER_RSP:
        case CMD_F2T_ADDR_REQ:
        case CMD_F2T_ADDR_RSP:
        case CMD_F2T_UPDATE_STREAM_REQ:
        case CMD_F2T_UPDATE_STREAM_RSP:
        case CMD_F2T_KEEP_ALIVE_REQ:
        case CMD_F2T_KEEP_ALIVE_RSP:

        // tracker<--->forward v2
        // 250~299
        case CMD_FS2T_REGISTER_REQ_V2:
        case CMD_FS2T_REGISTER_RSP_V2:
        case CMD_FC2T_ADDR_REQ_V2:
        case CMD_FC2T_ADDR_RSP_V2:
        case CMD_FS2T_UPDATE_STREAM_REQ_V2:
        case CMD_FS2T_UPDATE_STREAM_RSP_V2:
        case CMD_FS2T_KEEP_ALIVE_REQ_V2:
        case CMD_FS2T_KEEP_ALIVE_RSP_V2:

        // tracker<--->forward v3 (support long stream id)
        // 250~299
        case CMD_FS2T_REGISTER_REQ_V3:
        case CMD_FS2T_REGISTER_RSP_V3:
        case CMD_FC2T_ADDR_REQ_V3:
        case CMD_FC2T_ADDR_RSP_V3:
        case CMD_FS2T_UPDATE_STREAM_REQ_V3:
        case CMD_FS2T_UPDATE_STREAM_RSP_V3:
        case CMD_FS2T_KEEP_ALIVE_REQ_V3:
        case CMD_FS2T_KEEP_ALIVE_RSP_V3:

        // server stat
        case CMD_IC2T_SERVER_STAT_REQ:
        case CMD_IC2T_SERVER_STAT_RSP:
        // receiver<--->up_sche
        // 300~350
        case CMD_US2R_REQ_UP:
        case CMD_R2US_RSP_UP:
        case CMD_R2US_KEEPALIVE:

        // uploader<--->receiver
        // 350~399
        case CMD_U2R_REQ_STATE:
        case CMD_U2R_RSP_STATE:
        case CMD_U2R_STREAMING:
        case CMD_U2R_CMD:

        case CMD_U2R_REQ_STATE_V2:
        case CMD_U2R_RSP_STATE_V2:
        case CMD_U2R_STREAMING_V2:
        case CMD_U2R_CMD_V2:

        // mod_tracker  <--> tracker
        case CMD_MT2T_REQ_RSP:

        // rtp uploader <----> receiver
        case CMD_RTP_U2R_REQ_STATE:
        case CMD_RTP_U2R_RSP_STATE:
        case CMD_RTP_U2R_PACKET:
        case CMD_RTCP_U2R_PACKET:

        // rtp downloader <----> player
        case CMD_RTP_D2P_REQ_STATE:
        case CMD_RTP_D2P_RSP_STATE:
        case CMD_RTP_D2P_PACKET:
        case CMD_RTCP_D2P_PACKET:

        // rtp forward <----> forward
        case CMD_RTP_F2F_REQ_STATE:
        case CMD_RTP_F2F_START_STREAM: // fc2fs
        case CMD_RTP_F2F_STOP_STREAM: // fc2fs
        case CMD_RTP_F2F_RESULT: // fs2fc/fc2fs
        case CMD_RTP_F2F_NORM_DATA: // fs2fc/fc2fs
        case CMD_RTP_F2F_SDP: // fs2fc/fc2fs

        // rpc
        case CMD_RPC_REQ_STATE:
        case CMD_RPC_RSP_STATE:
        case CMD_RPC_KEEPALIVE_STATE:
            rtn = VALID_DATA;
            break;

        default:
            rtn = INVALID_DATA;
            DBG("CMDDataOpr::_is_valid: cmd %d invalid", cmd);
            break;
    }

    return rtn;
}
