import struct
import logging

MAX_PKG_SIZE = 1 * 1024 * 1024

comm_head_pat = '!BBHI'
# struct comm_head
# {
#     uint8_t  magic;
#     uint8_t  version;
#     uint16_t cmd;
#     uint32_t size;
# }

# typedef struct ip4_addr
# {
#     uint32_t ip;
#     uint16_t port;
# }ip4_addr;

s2s_dummy_cmd = 65500
s2s_dummy_pat = '!Q'
# typedef struct s2s_dummy_cmd
# {
# 	uint64_t pad;
# }s2s_dummy_cmd;


forward_stream_status_pat = '!IIIiQ'
# typedef struct forward_stream_status
# {
#     uint32_t streamid;
#     uint32_t player_cnt;
#     uint32_t forward_cnt;
#     int32_t keyframe_timestamp;
#     uint64_t block_seq;
# }forward_stream_status;

f2p_keepalive_cmd = 50
f2p_keepalive_pat = '!IHII'
# struct f2p_keepalive
# {
#     ip4_addr listen_player_addr;
#     uint32_t outbound_speed;
#     uint32_t stream_cnt;
#     forward_stream_status stream_status[stream_cnt];
# }

p2f_inf_stream_cmd = 51
p2f_inf_stream_pat = '!H'
p2f_inf_stream_val_pat = '!I'
# struct p2f_inf_stream
# {
#     uint16_t cnt;
#     uint32_t streamids[cnt];
# }

p2f_start_stream_cmd = 52
p2f_start_stream_pat = '!I'
# struct p2f_start_stream
# {
#     uint32_t streamid;
# }

p2f_close_stream_cmd = 53
p2f_close_stream_pat = '!I'
# struct p2f_close_stream
# {
#     uint32_t streamid;
# }


receiver_stream_status_pat = '!IIIQ'
# typedef struct receiver_stream_status
# {
#     uint32_t streamid;
#     uint32_t forward_cnt;
#     uint32_t keyframe_timestamp;
#     uint64_t block_seq;
# }receiver_stream_status;


r2p_keepalive_cmd = 100
r2p_keepalive_pat = '!IHIII'
# struct r2p_keepalive
# {
#     ip4_addr_t listen_uploader_addr;
#     uint32_t outbound_speed;
#     uint32_t inbound_speed;
#     uint32_t stream_cnt;
#     receiver_stream_status streams[stream_cnt];
# }





#return pkg size
def parse_pkg_len(buf):
    if len(buf) < struct.calcsize(comm_head_pat):
        return -1
    buf = buf[0:struct.calcsize(comm_head_pat)]
    try:
        # magic,version, cmd, size
        (_, _, _, size) = struct.unpack(comm_head_pat, buf)
        return size
    except struct.error:
        logging.exception("find exception")
    return -1


def gen_pkg(cmd, payload, size = 0, pkg_size = 0):
    if 0 == size:
        size = len(payload)
    if 0 == pkg_size:
        pkg_size = struct.calcsize(comm_head_pat)
        pkg_size += size
    pkg = struct.pack(comm_head_pat, 0xFF, 0x01,
                      cmd, pkg_size)
    return pkg + payload




u2us_req_addr_cmd = 150
u2us_req_addr_pat = '!IIIIQ32s32sIHB'
# typedef struct u2us_req_addr
# {
#     uint32_t version;     // uploader version
#     uint32_t uploader_code; // uploader code
#     uint32_t roomid;
#     uint32_t streamid;
#     uint64_t userid;
#     uint8_t sche_token[32];
#     uint8_t up_token[32];
#     uint32_t ip;
#     uint16_t port;
#     uint8_t stream_type;
# }u2us_req_addr;

us2u_rsp_addr_cmd = 151
us2u_rsp_addr_pat = '!IIHH32s'
# typedef struct us2u_rsp_addr
# {
#     uint32_t streamid;
#     uint32_t ip;
#     uint16_t port;
#     uint16_t result; // 0 standard for success or else for error code
#     uint8_t up_token[32];
# }us2u_rsp_addr;

u2us_req_addr_v2_cmd = 152
u2us_req_addr_v2_pat = '!IIQ32sBII'
# typedef struct u2us_req_addr_v2
# {
#     uint32_t roomid;
#     uint32_t streamid;
#     uint64_t userid;
#     uint8_t sche_token[32];
#     uint8_t stream_type;
#     uint32_t width;
#     uint32_t height;
# }u2us_req_addr_v2;

us2u_rsp_addr_v2_cmd = us2u_rsp_addr_cmd
us2u_rsp_addr_v2_pat = us2u_rsp_addr_pat


us2r_req_up_cmd = 300
us2r_req_up_pat = '!III32s'
# struct us2r_req_up
# {
#     uint32_t seq_id;
#     uint32_t streamid;
#     uint32_t userid;
#     char token[32];
# };


r2us_rsp_up_cmd = 301
r2us_rsp_up_pat = '!IIH'
# struct r2us_rsp_up
# {
#     uint32_t seq_id;
#     uint32_t streamid;
#     uint8_t result;
# };


r2us_keepalive_cmd = 100
r2us_keepalive_pat = r2p_keepalive_pat
#typedef struct r2p_keepalive r2us_keepalive;
