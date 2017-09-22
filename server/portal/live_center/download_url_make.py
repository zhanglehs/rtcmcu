#coding:utf-8 
import logging
from data_struct_defs import *
from gen_download_item import *
from stream_id_helper import StreamIDHelper
from token_helper import TokenHelper
import lctrl_conf



def make_pl_sche_token(stream_id, secret_word):
    curr_time = (int(time.time()) & 0xFFFFFFFF)
    ret = TokenHelper.make_token(curr_time, stream_id, '0', '', secret_word)
    return ret

def make_pl_sche_url(stream_id_32, url_pat, play_token = None, ver = 1, ret_stream_id = False):
    token = play_token
    if token == None:
       token = make_pl_sche_token(stream_id_32, lctrl_conf.pl_sche_secret)
    port_str = ''
    if 80 != lctrl_conf.pl_sche_server_port:
        port_str = ":%d" % (lctrl_conf.pl_sche_server_port)

    if ver == 1:
        url_pat %= (lctrl_conf.pl_sche_server_addr, port_str, token, stream_id_32)
    else:
        url_pat %= (lctrl_conf.pl_sche_server_addr, port_str, stream_id_32, token)

    if ret_stream_id == True:
        hex_short_stream_id = '%08x' % stream_id_32 
        return (hex_short_stream_id, url_pat)
    return url_pat

def make_pls_url(stream_id, format_type, ext_name ,play_token = None, 
                 is_long_stream_id = False, ret_stream_id = False):
    stream_id_128 = stream_id
    token = play_token
    if token == None:
        stream_id_32 = stream_id;
        if True == is_long_stream_id:
             helper = StreamIDHelper()
             (ret, stream_id_32, def_type, ver) = helper.decode(str(stream_id))
        token = make_pl_sche_token(stream_id_32, lctrl_conf.pl_sche_secret)
    else:
        pass

    tmp_url = ''
    pl_protocol_ver = 'v1'
    if False == is_long_stream_id:
        helper = StreamIDHelper(int(stream_id))
        stream_id_128 = helper.encode(DefiType.ORGIN)
    
    if "flv" in format_type:
        format_type = "f"

    if "sdp" in ext_name:
        tmp_url = 'download/sdp/%s?token=%s'
        tmp_url %= (stream_id_128, play_token)
    elif "m3u8" in ext_name:
        tmp_url = 'live/%s/%s/%s.%s?token=%s'
        tmp_url %= (format_type, pl_protocol_ver, stream_id_128, ext_name, play_token)
    elif "audio.flv" in ext_name:
        tmp_url = 'live/%s/%s/%s/audio?token=%s'
        tmp_url %= (format_type, pl_protocol_ver, stream_id_128, play_token)
    else:
        tmp_url = 'live/%s/%s/%s?token=%s'
        tmp_url %= (format_type, pl_protocol_ver, stream_id_128, play_token)
    url_pat = 'http://%s%s/' + tmp_url
    port_str = ''
    if 80 != lctrl_conf.pl_sche_server_port:
        port_str = ":%d" % (lctrl_conf.pl_sche_server_port)

    url_pat %= (lctrl_conf.pl_sche_server_addr, port_str)

    if ret_stream_id == True:
        return (stream_id_128, url_pat)
    return url_pat

def make_flv_url(stream_id, play_token = None, ver = 1, ret_stream_id = False):
    if ver == 1:
        url = 'http://%s%s/v1/ss?a=%s&b=%08x'
        return make_pl_sche_url(stream_id, url, play_token, ver, ret_stream_id)
    else:
        return make_pls_url(stream_id, 'flv',  'flv' ,play_token, False, ret_stream_id)

def make_flv_v2_url(stream_id, play_token = None, ver = 1, ret_stream_id = False):
    return make_pls_url(stream_id, 'flv',  'flv' ,play_token, False, ret_stream_id)

def make_rtp_url(stream_id, play_token = None, ver = 1, ret_stream_id = False):
    return make_pls_url(stream_id, 'rtp',  'sdp' ,play_token, False, ret_stream_id)
 
def make_rtmp_v1_url(stream_id, play_token = None, ver = 1, ret_stream_id = False):
    return make_pls_url(stream_id, 'flv',  'flv' ,play_token, False, ret_stream_id)
    
def make_ts_url(stream_id_32, play_token = None, ver = 1, ret_stream_id = False, defi_type = DefiType.ORGIN):
    helper = StreamIDHelper(stream_id_32)
    stream_id_128 = helper.encode(defi_type)
    return make_pls_url(stream_id_128, 'hls',  'm3u8' ,play_token, True, ret_stream_id)

def make_ts_378_16_9_url(stream_id_32, play_token = None, ver = 1, ret_stream_id = False):
    return make_ts_url(stream_id_32, play_token, ver, ret_stream_id, DefiType.SMOOTH)

def make_ts_540_16_9_url(stream_id_32, play_token = None, ver = 1, ret_stream_id = False):
    return make_ts_url(stream_id_32, play_token, ver, ret_stream_id, DefiType.SD)

def make_ts_720_16_9_url(stream_id_32, play_token = None, ver = 1, ret_stream_id = False):
    return make_ts_url(stream_id_32, play_token, ver, ret_stream_id, DefiType.HD)

def make_audio_url(stream_id, play_token = None, ret_stream_id = False):
    return make_pls_url(stream_id, 'flv',  'audio.flv' ,play_token, False, ret_stream_id)

def make_flv_url_v2(stream_id_32, defi_type, play_token = None, ver = 1, ret_stream_id = False):
    helper = StreamIDHelper(stream_id_32)
    stream_id_128 = helper.encode(defi_type)
    return make_pls_url(stream_id_128, 'flv', 'flv' , play_token, True, ret_stream_id)

def make_flv_720_16_9_url(stream_id_32, play_token = None, ver = 1, ret_stream_id = False):
    return make_flv_url_v2(stream_id_32, DefiType.HD, play_token, ver, ret_stream_id)

def make_flv_540_16_9_url(stream_id_32, play_token = None, ver = 1, ret_stream_id = False):
    return make_flv_url_v2(stream_id_32, DefiType.SD, play_token, ver, ret_stream_id)

def make_flv_378_16_9_url(stream_id_32, play_token = None, ver = 1, ret_stream_id = False):
    return make_flv_url_v2(stream_id_32, DefiType.SMOOTH, play_token, ver, ret_stream_id)

def gen_download_item(type, url, format, res, rt, av, token, stream_id,
                      p2p_state, out_lst, out_lst_std, is_standard = True, format_ver = 2):
    if out_lst == None:
       out_lst = []
    out_lst.append({"type": str(type),
                   "url": str(url),      
                   "format": str(format), 
                   "res": str(res),    
                   "rt": str(rt), 
                   "av": str(av), 
                   "token": str(token), 
                   "stream_id": str(stream_id),
                   "p2p": str(p2p_state)
                   })

    if is_standard == True:
       is_origin = '0'
       if out_lst_std == None:
           out_lst_std = []

       type_std = str(type)
       format_std = str(format)
       if type == 'flv_v2' or type == 'ts' or type == 'rtp':
          is_origin = '1'
          format_std = 'flv'
       else:
          is_origin = '0'

       if type == 'rtp':
          format_std = 'sdp'

       elif type == 'ts':
          format_std = 'm3u8'

       else:
           pass
          
       new_url = url
       #new_url = lutil.url_values_del(url, ['token'])


       out_lst_std.append({
                   "url": new_url,      
                   "format": format_std, 
                   "res": str(res),    
                   "rt": str(rt), 
                   "av": str(av), 
                   "stream_id": str(stream_id),
                   "p2p": str(p2p_state),
                   "format_ver": str(format_ver),
                   "origin": is_origin
                   })

    return (out_lst, out_lst_std)

