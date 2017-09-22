#coding:utf-8 
'''
@file download_list_make.py
@brief make.
       It can be used to generate download url list.
@author renzelong
<pre><b>copyright: Youku</b></pre>
<pre><b>email: </b>renzelong@youku.com</pre>
<pre><b>company: </b>http://www.youku.com</pre>
<pre><b>All rights reserved.</b></pre>
@date 2015/09/23
@see  ./unit_tests/download_list_make_test.py \n
'''
import logging
from data_const_defs import *
from download_url_make import *
from lctrl_utils import *

def get_p2p_val(p2p_param_SD):
    p2p_val = "0"
    if p2p_param_SD != '' and ("p2p=1" in p2p_param_SD) == True:
        p2p_val = "1"
    return p2p_val

def gen_rtp_play_sche_url(pl_token, res, rt, stream_id, p2p_state, out_lst, out_lst_std, 
                           is_need_ret_stream_id, ver = 2, ext_para_dict = DEFAULT_EXT_PARAM_DICT):
    try:
        (rtp_stream_id, rtp_url) =  make_rtp_url(stream_id, pl_token, ver, is_need_ret_stream_id)
        rtp_url += ext_para_dict['append_content'] 
        gen_download_item('rtp',rtp_url , 'rtp', res, rt, 'av', pl_token, rtp_stream_id, 
                          p2p_state, out_lst, out_lst_std, True, 1)
    except Exception as e:
        logging.exception('gen_rtp_play_sche_url Exception:%s', str(e))

def gen_fc_play_sche_url(pl_token, res, rt, stream_id, p2p_state, out_lst, out_lst_std, 
                         is_need_ret_stream_id, ver = 2, ext_para_dict = DEFAULT_EXT_PARAM_DICT):
    try:
        (flv_stream_id, flv_url)     = make_flv_url(stream_id, pl_token, 1, is_need_ret_stream_id)
        (flv_v2_stream_id, flv_v2_url)  = make_flv_v2_url(stream_id, pl_token, ver, is_need_ret_stream_id)
        (rtmp_v1_stream_id, rtmp_v1_url) = make_rtmp_v1_url(stream_id, pl_token, 1, is_need_ret_stream_id)
        flv_url += ext_para_dict['append_content'] 
        flv_v2_url += ext_para_dict['append_content'] 
        rtmp_v1_url += ext_para_dict['append_content'] 

        gen_download_item('flv',flv_url , 'flv', res, rt, 'av', pl_token, flv_stream_id, 
                          p2p_state,out_lst, out_lst_std, False)
        gen_download_item('flv_v2',flv_v2_url , 'flv_v2', res, rt, 'av', pl_token, flv_v2_stream_id,
                          p2p_state, out_lst, out_lst_std, True)
        gen_download_item('rtmp_v1',rtmp_v1_url , 'rtmp', res, rt, 'av', pl_token, rtmp_v1_stream_id, 
                          p2p_state, out_lst, out_lst_std, True, 1)
    except Exception as e:
         logging.exception('gen_fc_play_sche_url Exception:%s', str(e))

def gen_play_sche_url_720(pl_token, res, rt, stream_id, p2p_state, out_lst, out_lst_std, 
                              is_need_ret_stream_id, stream_type, ver = 2, p2p_param_SD = '',  
                              ext_para_dict = DEFAULT_EXT_PARAM_DICT):

    (ret_stream_id_net, ret_url_net) = make_flv_720_16_9_url(stream_id, pl_token, ver, is_need_ret_stream_id)
    (ret_stream_id_flv_origin, ret_url_flv_origin) = make_flv_v2_url(stream_id, pl_token, ver,
                                                                             is_need_ret_stream_id)
    p2p_val = get_p2p_val(p2p_param_SD)
    ret_url_net +=  ext_para_dict['append_content'] 
    ret_url_flv_origin +=  ext_para_dict['append_content'] 
    if stream_type == 'ns':
        gen_download_item('qa_3', ret_url_net, 'flv', res, VIDEO_RT[DefiType.ORGIN], 
                          'av', pl_token, ret_stream_id_net, p2p_state, out_lst, out_lst_std)
    else:
        gen_download_item('qa_3', ret_url_flv_origin, 'flv', res, VIDEO_RT[DefiType.ORGIN], 'av', 
                          pl_token, ret_stream_id_flv_origin, p2p_state, out_lst, out_lst_std)

    (ret_stream_id_flv_960, ret_url_flv_960) = make_flv_540_16_9_url(stream_id, pl_token, ver, 
                                                                             is_need_ret_stream_id)
    ret_url_flv_960 +=  ext_para_dict['append_content']
    gen_download_item('qa_2', ret_url_flv_960 + p2p_param_SD, 'flv', VIDEO_RES[DefiType.SD],
                              VIDEO_RT[DefiType.SD], 'av', pl_token, ret_stream_id_flv_960, 
                              p2p_val, out_lst, out_lst_std)

    (ret_stream_id, ret_url) = make_flv_378_16_9_url(stream_id, pl_token, ver, is_need_ret_stream_id)
    ret_url +=  ext_para_dict['append_content']
    gen_download_item('qa_1', ret_url + p2p_param_SD, 'flv', VIDEO_RES[DefiType.SMOOTH], 
                              VIDEO_RT[DefiType.SMOOTH], 'av', pl_token, ret_stream_id, p2p_val, out_lst, out_lst_std)


def gen_play_sche_url_540(pl_token, res, rt, stream_id, p2p_state, out_lst, out_lst_std, is_need_ret_stream_id, 
        stream_type, ver = 2, p2p_param_SD = '', ext_para_dict = DEFAULT_EXT_PARAM_DICT):
    ret_stream_id = None
    ret_url = None

    p2p_val = get_p2p_val(p2p_param_SD)
    if stream_type == 'ns':
         (ret_stream_id, ret_url) = make_flv_540_16_9_url(stream_id, pl_token, ver, is_need_ret_stream_id)
    else:
         (ret_stream_id, ret_url) = make_flv_v2_url(stream_id, pl_token, ver, is_need_ret_stream_id)
    
    ret_url +=  ext_para_dict['append_content']
    gen_download_item('qa_2', ret_url, 'flv', res, VIDEO_RT[DefiType.SD], 'av', pl_token,
                      ret_stream_id, p2p_val, out_lst, out_lst_std)

    (ret_stream_id, ret_url) = make_flv_378_16_9_url(stream_id, pl_token, ver, is_need_ret_stream_id)
    ret_url +=  ext_para_dict['append_content']

    gen_download_item('qa_1', ret_url + p2p_param_SD, 'flv', VIDEO_RES[DefiType.SMOOTH], 
                      VIDEO_RT[DefiType.SMOOTH], 'av', pl_token, ret_stream_id, p2p_val, out_lst, out_lst_std)

def gen_play_sche_url_origin(pl_token, res, rt, stream_id, p2p_state, out_lst, out_lst_std, is_need_ret_stream_id, 
        stream_type, ver, p2p_param_SD, need_transcoding, height, ext_para_dict = DEFAULT_EXT_PARAM_DICT, out_ver = 1):

    (flv_stream_id, flv_url)     = make_flv_url(stream_id, pl_token, 1, is_need_ret_stream_id)
    (flv_v2_stream_id, flv_v2_url)  = make_flv_v2_url(stream_id, pl_token, ver, is_need_ret_stream_id)
    #(ts_stream_id, ts_url)      = make_ts_url(stream_id, pl_token, ver, is_need_ret_stream_id)

    flv_url +=  ext_para_dict['append_content']
    flv_v2_url +=  ext_para_dict['append_content']
    #ts_url +=  ext_para_dict['append_content']

    gen_download_item('flv', flv_url, 'flv', res, rt, 'av', pl_token, flv_stream_id,
                      p2p_state, out_lst, out_lst_std, False)
    gen_download_item('flv_v2', flv_v2_url, 'flv', res, rt, 'av', pl_token, flv_v2_stream_id,
                      p2p_state, out_lst, out_lst_std)

    gen_play_sche_url_ts(pl_token, res, rt, stream_id, p2p_state, out_lst, out_lst_std,
                         is_need_ret_stream_id, stream_type, ver, p2p_param_SD, need_transcoding,
                         height, ext_para_dict, out_ver)


def gen_play_sche_url_ts(pl_token, res, rt, stream_id, p2p_state, out_lst, out_lst_std, is_need_ret_stream_id, 
        stream_type, ver, p2p_param_SD, need_transcoding, height, ext_para_dict = DEFAULT_EXT_PARAM_DICT, out_ver = 1):
    ts_stream_id = None
    ts_url = None
    ts_res = res
    ts_rt = rt
    if (height >= 720 or height >= 540) and (1 == need_transcoding):
        if out_ver == 1:
            if height >= 720: 
	         ts_rt  = VIDEO_RT[DefiType.SD]
                 ts_res = VIDEO_RES[DefiType.SD]
	         (ts_stream_id, ts_url) = make_ts_540_16_9_url(stream_id, pl_token, ver, is_need_ret_stream_id)
	    else:
                 ts_rt  = VIDEO_RT[DefiType.SMOOTH]
                 ts_res = VIDEO_RES[DefiType.SMOOTH]
		 (ts_stream_id, ts_url) = make_ts_378_16_9_url(stream_id, pl_token, ver, is_need_ret_stream_id)
        else:
            (ts_stream_id, ts_url) = make_ts_url(stream_id, pl_token, ver, is_need_ret_stream_id)
            gen_download_item('ts', ts_url + ext_para_dict['append_content'], 'ts',res, rt, 
                       'av', pl_token, ts_stream_id, p2p_state, out_lst, out_lst_std, True, 1)

            if height >= 720:
               (ts_stream_id, ts_url) = make_ts_540_16_9_url(stream_id, pl_token, ver, is_need_ret_stream_id)
               gen_download_item('ts', ts_url + ext_para_dict['append_content'], 'ts', VIDEO_RES[DefiType.SD], VIDEO_RT[DefiType.SD], 
                       'av', pl_token, ts_stream_id, p2p_state, out_lst, out_lst_std, True, 1)

            if height >= 540:
               (ts_stream_id, ts_url) = make_ts_378_16_9_url(stream_id, pl_token, ver, is_need_ret_stream_id)
               gen_download_item('ts', ts_url + ext_para_dict['append_content'], 'ts', VIDEO_RES[DefiType.SMOOTH], VIDEO_RT[DefiType.SMOOTH], 
                       'av', pl_token, ts_stream_id, p2p_state, out_lst, out_lst_std, True, 1)
            return 
    else:
        (ts_stream_id, ts_url) = make_ts_url(stream_id, pl_token, ver, is_need_ret_stream_id)

    ts_url +=  ext_para_dict['append_content']
    gen_download_item('ts', ts_url, 'ts', ts_res, ts_rt, 'av', pl_token, ts_stream_id, p2p_state, out_lst, out_lst_std, True, 1)

def gen_play_sche_url_audio(pl_token, res, rt, stream_id, p2p_state, out_lst, out_lst_std, 
                            ext_para_dict = DEFAULT_EXT_PARAM_DICT):
    (audio_stream_id, audio_url) = make_audio_url(stream_id, pl_token, True)
    audio_url +=  ext_para_dict['append_content']
    gen_download_item('flv', audio_url, 'flv', '', AUDIO_RT[0], 'a', pl_token, audio_stream_id,
                      p2p_state, out_lst, out_lst_std, True, 1)

def generate_play_sche_url(stream_type,stream_id, pl_token, rt, resolution, need_transcoding, 
                           client_type, ver, p2p_flag = "0", ext_para_dict = DEFAULT_EXT_PARAM_DICT, out_ver = 1):
    ps_dict = []
    ps_dict_std = []
    is_need_ret_stream_id = True
    resolution_arr = resolution.lower().split('x')
    height = int(resolution_arr[1])
    res = str(resolution_arr[0]) +"x"+ str(resolution_arr[1])
    p2p_param = ''
    p2p_param_SD = ''
    p2p_state_val = "0"
    p2p_state_val_defalut = "0"
    p2p_flag = str(p2p_flag)
    if p2p_flag != '':
        p2p_param_SD = '&p2p=' + p2p_flag
        p2p_param = p2p_param_SD
        p2p_state_val = p2p_flag 

    if 'fc' == client_type:
        gen_fc_play_sche_url(pl_token, res, rt, stream_id,
                             p2p_state_val_defalut, ps_dict, ps_dict_std, is_need_ret_stream_id, ver, ext_para_dict)
        return (ps_dict, ps_dict_std)

    elif 'rtp' == client_type:
        gen_rtp_play_sche_url(pl_token, res, rt, stream_id, p2p_state_val_defalut,
                                      ps_dict, ps_dict_std, is_need_ret_stream_id, ver, ext_para_dict)
        #return (ps_dict, ps_dict_std)
    
    if need_transcoding == 1:#need transcoding
        if height >= 720:
            gen_play_sche_url_720(pl_token, res, rt, stream_id, p2p_state_val_defalut, 
                                          ps_dict, ps_dict_std, is_need_ret_stream_id, stream_type, ver,
                                          p2p_param_SD, ext_para_dict)
        elif height >= 540:
            gen_play_sche_url_540(pl_token, res, rt, stream_id, p2p_state_val_defalut, 
                                          ps_dict, ps_dict_std, is_need_ret_stream_id, stream_type, ver, 
                                          p2p_param_SD, ext_para_dict)

    gen_play_sche_url_audio(pl_token, res, rt, stream_id, p2p_state_val_defalut, 
                            ps_dict, ps_dict_std, ext_para_dict)

    gen_play_sche_url_origin(pl_token, res, rt, stream_id, p2p_state_val_defalut, 
                             ps_dict, ps_dict_std, is_need_ret_stream_id, stream_type, ver, 
                             p2p_param_SD, need_transcoding, height, ext_para_dict, out_ver)
    return (ps_dict, ps_dict_std)

#return (old, string_error_code_json_str, int_error_code_json_str)
def make_get_playlist(create_req, ext_para_dict = DEFAULT_EXT_PARAM_DICT, ver = 1):
    ps_dict = {}
    ps_dict_std = {}
    try:
        
        (ps_dict, ps_dict_std) = generate_play_sche_url(create_req.upload_client_type,
                                                        create_req.stream_id,
                                                        create_req.play_token, 
                                                        create_req.rt, 
                                                        create_req.res, 
                                                        create_req.need_transcoding, 
                                                        create_req.upload_client_type, 
                                                        create_req.potocol_ver,
                                                        create_req.p2p_flag, ext_para_dict, ver)
    except Exception as e:
        logging.exception('make_get_playlist exception:%s', str(e))
        return (None, None)

    tmp_dict = {}
    tmp_dict['download_list'] = ps_dict_std
    string_errcode_json_play_list = generate_json_ex_v2(ECODE_SUCC, tmp_dict, ver == 1)
    int_errcode_json_play_list = generate_json_ex_v2(ECODE_SUCC, tmp_dict, False)
    return (ps_dict, string_errcode_json_play_list, int_errcode_json_play_list)



