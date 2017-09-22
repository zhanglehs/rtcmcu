#coding:utf-8
import ujson
import lversion
import time
import logging

from data_const_defs import *

def generate_json(error_code, extra_dict = None):
    return generate_json_ex(error_code, extra_dict, '1.0')


def generate_json_for_value(error_code, para_name, para_value, ver_str = '1.0'):
    json_dict = {}
    json_dict[str(para_name)] = para_value
    json_dict['error_code'] = str(error_code)
    return ujson.encode(json_dict)

def generate_json_ex(error_code, extra_dict, ver_str = '1.0', is_str_err_code = True):
    json_dict = {}
    if None != extra_dict:
        json_dict = dict(extra_dict)
    json_dict['version'] = ver_str
    if is_str_err_code:
	json_dict['error_code'] = str(error_code)
    else:
        json_dict['error_code'] = int(error_code)
    return ujson.encode(json_dict)

def generate_json_ex_v2(error_code, data_dict, is_str_err_code = True):
    json_dict = {}
    if None != data_dict:
        json_dict = data_dict

    if is_str_err_code:
        json_dict['error_code'] = str(error_code)
    else:
        json_dict['error_code'] = int(error_code)
    return ujson.encode(json_dict)

def generate_json_v2(error_code, extra_dict, old_str, ext_str = ''):
    json_dict = {}
    if None != extra_dict:
        json_dict = dict(extra_dict)
    json_dict['error_code'] = str(error_code)
    json_dict['ext'] = ext_str
    json_dict['old'] = old_str
    return ujson.encode(json_dict)

def generate_json_for_create_stream(error_code, extra_dict, old_dict, ext_str = ''):
     try:
         return generate_json_for_create_stream_none_safe(error_code, extra_dict, old_dict, ext_str)
     except Exception as e:
         logging.exception('generate_json_for_create_stream exception:%s', str(e))
     return ujson.encode({})
 
def generate_json_for_create_stream_none_safe(error_code, extra_dict, old_dict, ext_str = ''):
     json_dict = {}
     if None != extra_dict:
         json_dict = dict(extra_dict)
     json_dict['error_code'] = error_code
     json_dict['ext'] = ext_str
 
     if old_dict != None:
         old_dict['version'] = '2.0'
         old_dict['error_code'] = '0'
         json_dict['old'] = old_dict
     return ujson.encode(json_dict)

def err_process(respose_ver, err_code, result = False, ret_json = None, 
                ext_params = {'stream_id': -1, 'alias': ''}):
    if respose_ver == 2:
        if ret_json == None:
            ret_json = generate_json_ex(err_code, None, '2.0')
    else:
        if ret_json == None:
            ret_json = generate_json_v2(err_code, None, '')
    return (result, ext_params, ret_json)

def format_http_header(code, content_len, extra_header = "", cont_type = "text/plain"):
    msg = HTTP_MSG.get(code, 'Unknown')
    return "HTTP/1.1 %d %s\r\nServer:%s\r\nContent-Length:%d\r\nConnection:close\r\nContent-Type:%s\r\n%s\r\n" % (code, msg, lversion.lc_server, content_len, cont_type, extra_header)

def parse_req_header(header_buf):
    idx = header_buf.find("\r\n")
    if -1 == idx:
        return ('', {})
    first_line = header_buf[0:idx].strip()
    left = header_buf[idx:].strip()
    lines = left.split('\r')
    ret = {}
    for l in lines:
        l = l.strip()
        (k, _, v) = l.partition(':')
        k = k.strip().lower()
        if 0 == len(k):
            continue
        ret[k] = v.strip()

    return (first_line, ret)

def parse_rsp_header(http_head):
    line_ls = http_head.split('\r')
    ret_code = 0
    first_line = line_ls[0]
    ls = first_line.split()
    try:
        ret_code = int(ls[1])
    except ValueError:
        return (0, -1)

    for line in line_ls:
        line = line.strip()
        sections = line.split(':')
        if len(sections) != 2:
            continue
        if 'content-length' == sections[0].strip().lower():
            try:
                return (ret_code, int(sections[1].strip()))
            except ValueError:
                pass

    return (ret_code, -1)


def is_invalid_para_str_v2(para):
    try:
        if para == None:
            return True

        para_str = str(para).strip()
        if para_str == '':
            return True
    except:
        return True

    return False

def is_invalid_para(para):
    ret = is_invalid_para_v2(para)
    return ret

def is_invalid_para_v2(para):
    para = para.strip()

    try:
        val = int(para)
        if val <= 0:
            return True
    except:
        return True

    if len(para) <= 0:
        return True

    if -1 != para.find(lstream.DATA_SEP_CHR):
        return True

    return False

def is_invalid_para_str(para):
    ret = is_invalid_para_str_v2(para)
    return ret

def is_valid_stream_type(stream_type_str):
    if True == is_invalid_para_str(stream_type_str):
        return False;
    supported_stream_type_lst = ['pc_plugin', 'fc', 'ns', 'rtmp', 'rtp']
    return stream_type_str.lower() in supported_stream_type_lst 

def is_valid_stream_format(format_type_str):
    if True == is_invalid_para_str(format_type_str):
        return False;
    return True#'rtmp' == format_type_str  or 'mms' == format_type_str or o or 'mms' == format_type_str or r 'mms' == format_type_str or 'flv' == format_type_str

def is_valid_res(res):
    if False == is_invalid_para_str(res):
        arr = res.lower().split('x')
        if len(arr) < 2:
            return False
        return str(arr[0]).isdigit() and str(arr[1]).isdigit()
    return False

def is_valid_rt(rt):
    ret = (False == is_invalid_para_int(rt))
    return ret

def is_valid_need_transcoding(param):
    ret = (False == is_invalid_para_int(param))
    if ret == True:
        temp = int(param)
        if temp != 0 and temp != 1:
            ret = False
    return ret

