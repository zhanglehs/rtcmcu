#coding:utf-8 
import socket
import lutil
import struct
import logging
import time
from stream_manager import * 
from streaminfo_pack import StreamInfoListPack, StreamInfoPack

stream_info_cmd_v2_cache = None #cache stream white list msg v2


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

forward_stream_status_pat = '!IIIiq'
# typedef struct forward_stream_status
# {
#     uint32_t streamid;
#     uint32_t player_cnt;
#     uint32_t forward_cnt;
#     int32_t keyframe_timestamp;
#     int64_t block_seq;
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

timeval_pat = 'qq' 
# struct timeval
# {
#     int64_t second;
#     int64_t microsecond;
# }

stream_info_pat = '!Iqq'#
# struct stream_info
# {
#     uint32_t streamid;
#     timval start_time;
# }

p2f_inf_stream_cmd_v2 = 54
p2f_inf_stream_pat_v2 = '!H'
p2f_inf_stream_val_pat_v2 = '!II'#'format = "!H%ds" % len(stream_info)
# struct p2f_inf_stream
# {
#     uint16_t cnt;
#     stream_info streamids[cnt];
# }

p2f_start_stream_cmd_v2 = 55
p2f_start_stream_pat_v2 = stream_info_pat
# typedef stream_info p2f_start_stream  


#request white list for interlive(receiver,forward-pf,forward)
f2p_white_list_req = 56
p2f_white_list_res = p2f_inf_stream_cmd_v2
p2f_white_list_res_pat = p2f_inf_stream_val_pat_v2


receiver_stream_status_pat = '!IIIq'
# typedef struct receiver_stream_status
# {
#     uint32_t streamid;
#     uint32_t forward_cnt;
#     uint32_t keyframe_timestamp;
#     int64_t block_seq;
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


def gen_pkg(cmd, payload, size = 0):
    if 0 == size:
        size = len(payload)
    pkg_size = struct.calcsize(comm_head_pat)
    pkg_size += size
    pkg = struct.pack(comm_head_pat, 0xFF, 0x01,
                      cmd, pkg_size)
    return pkg + payload


def gen_stream_list_pkg_v3(sm):
    try:
        global stream_info_cmd_v2_cache
        if None == stream_info_cmd_v2_cache or \
            True == sm.get_stream_info_bin_changed_flag():
            sm.reset_stream_info_bin_changed_flag()
            payload = ''
            stream_info_list_pack = StreamInfoListPack()
            stream_info_array = sm.get_stream_info_list_v2()
            payload = stream_info_list_pack.pack(stream_info_array)
            stream_info_cmd_v2_cache = gen_pkg(p2f_inf_stream_cmd_v2, payload)
            #logging.debug("gen_stream_list_pkg_v3 stream_info_cmd_v2_cache len:%s", len(str(stream_info_cmd_v2_cache)))
        return stream_info_cmd_v2_cache

    except ValueError as e:
        logging.exception("gen_stream_list_pkg_v3 find ValueError.err:" + str(e))
        return ''
    except struct.error as e:
        logging.exception("gen_stream_list_pkg_v3 find struct.error.err:" + str(e))
        return ''
    except Exception as e:
        logging.exception("gen_stream_list_pkg_v3 find Exception.err:" + str(e))
    return '' 

def gen_stream_start_pkg_v2(stream_id, start_time):
    payload = ''
    try:
        stream_info_pack = StreamInfoPack(stream_id, start_time)
        payload = stream_info_pack.pack()
    except ValueError as e:
        logging.exception("gen_stream_start_pkg_v2 find ValueError.err:" + str(e))
        return ''
    except struct.error as e:
        logging.exception("gen_stream_start_pkg_v2 find struct.error.err:" + str(e))
        return ''
    except Exception as e:
        logging.exception("gen_stream_start_pkg_v2 find Exception.err:" + str(e))
        return ''
    return gen_pkg(p2f_start_stream_cmd_v2, payload)

def gen_stream_stop_pkg_v2(stream_id):
    payload = ''
    try:
        payload = struct.pack(p2f_close_stream_pat, stream_id)
    except struct.error as e:
        logging.exception("gen_stream_stop_pkg_v2 can't pack close_stream." + str(stream_id) + ' exception:' + str(e))
        return ''
    return gen_pkg(p2f_close_stream_cmd, payload)



