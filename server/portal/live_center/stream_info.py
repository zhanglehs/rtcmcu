#coding:utf-8 
import logging
import time
import ujson
from timeval_t import timeval_t

class StreamInfo():
    """description of class"""
    def __init__(self):
        self.stream_id = 0
        self.app_id = ''
        self.alias = ''
        self.start_time = timeval_t()
        self.update_time = int(time.time())
        self.nt = 0
        self.block_seq = -1

        self.delete_flag = 0
        self.delete_start_time = int(time.time())

        self.net_stream_upsche_addr = ''
        self.net_stream_src_url = ''
        self.upload_client_type = 'pc_plugin'
        self.download_stream_type = 'flv'
        self.res = ''
        self.rt = 400
        self.upload_url_json_str = ''
        self.play_list_json_str = ''
        self.play_list_json_str_v2 = ''
        self.p2p = 0

    def __str__(self):
        str_info = ' stream_id:' +  str(self.stream_id)
        str_info += ' app_id:' +  str(self.app_id)
        str_info += ' alias:' +  str(self.alias)
        str_info += ' start_time:' +  str(self.start_time)
        str_info += ' update_time:' +  str(self.update_time)
        str_info += ' nt:' +  str(self.nt)
        str_info += ' block_seq:' +  str(self.block_seq)
        str_info += ' delete_flag:' +  str(self.delete_flag)
        str_info += ' delete_start_time:' +  str(self.delete_start_time)
        str_info += ' net_stream_upsche_addr:' +  str(self.net_stream_upsche_addr)
        str_info += ' net_stream_src_url:' +  str(self.net_stream_src_url)
        str_info += ' upload_client_type:' +  str(self.upload_client_type)
        str_info += ' res:' +  str(self.res)
        str_info += ' rt:' +  str(self.rt)
        str_info += ' upload_url_json_str:' +  str(self.upload_url_json_str)
        str_info += ' play_list_json_str:' +  str(self.play_list_json_str)
        str_info += ' play_list_json_str_v2:' +  str(self.play_list_json_str_v2)
        str_info += ' p2p:' +  str(self.p2p)

        return str_info

    def local_update(self, other):
        if (other != None) and (not isinstance(other, StreamInfo)):
            return False

        if self.delete_flag != other.delete_flag:
            self.delete_flag = other.delete_flag
            self.delete_start_time = other.delete_start_time

        return True

    def to_dict(self):
        try:
            return self.to_dict_none_safe()

        except Exception as e:
             logging.exception("StreamInfo::to_dict exception:" + str(e))
             return ''
        return ''

    def to_dict_none_safe(self):
        d = {}
        d['stream_id'] = self.stream_id
        d['app_id'] = self.app_id
        d['alias'] = self.alias
        d['start_time'] = self.start_time.to_dict()
        d['update_time'] = timeval_t.timestamp2str(self.update_time)
        d['upload_client_type'] = self.upload_client_type
        d['download_stream_type'] = self.download_stream_type
        d['res'] = self.res
        d['rt'] = int(self.rt)
        d['nt'] = self.nt
        d['delete_flag'] = self.delete_flag
        d['delete_start_time'] = self.delete_start_time
        d['upload_url_json_str'] = self.upload_url_json_str
        d['play_list_json_str'] = self.play_list_json_str 
        d['play_list_json_str_v2'] = self.play_list_json_str_v2 
        d['net_stream_upsche_addr'] = self.net_stream_upsche_addr
        d['net_stream_src_url'] = self.net_stream_src_url
        d['block_seq'] = self.block_seq
        d['p2p'] = int(self.p2p)

        return d

    def safe_get(self, d, key, default_val, is_str = False):
        ret_val = default_val
        try:
            if is_str:
                ret_val = str(d[key])
            else:
                ret_val = int(d[key])
        except Exception as e:
             pass
        return ret_val

    def from_dict(self, d):
        try:
           self.stream_id = int(d['stream_id'])
           self.start_time.from_dict(d['start_time'])
           self.update_time = int(time.time())#timeval_t.str2timestamp(str(d['update_time']))
           
           self.res = d['res']
           self.rt = int(d['rt'])
           self.nt = self.safe_get(d, 'nt', 0, False)

           self.delete_flag = int(d['delete_flag'])
           
           self.upload_url_json_str = self.safe_get(d, 'upload_url_json_str', '', True)
           if '' == self.upload_url_json_str:
               self.upload_url_json_str = self.safe_get(d, 'upload_url', '', True)

           self.play_list_json_str = d['play_list_json_str']
           self.play_list_json_str_v2 = self.safe_get(d, 'play_list_json_str_v2', '', True)

           self.app_id = self.safe_get(d, 'app_id', 0, False)
           if 0 == self.app_id:
               self.app_id = self.safe_get(d, 'app_id_str', 0, False)

           self.alias = self.safe_get(d, 'alias', '', True)
           self.upload_client_type = self.safe_get(d, 'upload_client_type', 'pc_plugin', True)
           self.download_stream_type = self.safe_get(d, 'download_stream_type', 'flv', True)
           self.net_stream_upsche_addr = self.safe_get(d, 'net_stream_upsche_addr', '', True)
           self.net_stream_src_url = self.safe_get(d, 'net_stream_src_url', '', True)
           self.delete_start_time = self.safe_get(d, 'delete_start_time', -1, False)
           self.block_seq = self.safe_get(d, 'block_seq', -1, False)

           self.p2p = self.safe_get(d, 'p2p', 0, False)


        except Exception as e:
             logging.exception("StreamInfo::from_dict exception:" + str(e))
             return False
        return True
 
    def to_json_str(self):
         d = self.to_dict()
         return ujson.encode(d)

    def from_json_str(self, json_str):
        try:
            d = ujson.decode(json_str)
            self.from_dict(d)
            return True
        except Exception as e:
            logging.exception("StreamInfo::from_json_str exception when decode %s exception:%s", json_str, str(e))

        return False
