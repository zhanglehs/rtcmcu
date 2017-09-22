#coding:utf-8
import os
import time
from data_const_defs import *

class test_import:
    def __init__(self):
        pass
    def ok(self):
        pass

#create stream request info
class CreateStreamRequest:
    def __init__(self):
        self.stream_id          = 0
        self.upload_client_type = 'pc_plugin'
        self.net_stream_media_src_type = 'flv'
        self.res                = ''
        self.upload_ip_str      = ''
        self.upload_port_str    = ''
        self.stream_type        = ''
        self.hd_str             = ''
        self.app_id             = 0
        self.rt                 = 0
        self.need_transcoding   = 0 #1:need supported
        self.net_stream_upsche_addr = ''
        self.net_stream_media_src = ''
        self.alias              = ''
        self.p2p_flag           = 0
        self.potocol_ver        = 2  
        self.play_token         = None
        self.user_id_str        = '2'
        self.room_id_str        = '2'

    def __str__(self):
        attr_str = 'alias:' + self.alias
        attr_str += ' stream_id:' + str(self.stream_id)
        attr_str += ' upload_client_type:' + str(self.upload_client_type)
        attr_str += ' net_stream_media_src_type:' + str(self.net_stream_media_src_type)
        attr_str += ' res:' + str(self.res)
        attr_str += ' upload_ip_str:' + str(self.upload_ip_str)
        attr_str += ' upload_port_str:' + str(self.upload_port_str)
        attr_str += ' stream_type:' + str(self.stream_type)
        attr_str += ' hd_str:' + str(self.hd_str)
        attr_str += ' app_id:' + str(self.app_id)
        attr_str += ' rt:' + str(self.rt)
        attr_str += ' need_transcoding:' + str(self.need_transcoding)
        attr_str += ' p2p_flag:' + str(self.p2p_flag)
        attr_str += ' potocol_ver:' + str(self.potocol_ver)
        attr_str += ' play_token:' + str(self.play_token)
        attr_str += ' net_stream_upsche_addr:' + str(self.net_stream_upsche_addr)
        attr_str += ' net_stream_media_src:' + str(self.net_stream_media_src)

        return attr_str

#destroy stream request info
class DestroyStreamRequest:
    def __init__(self):
        self.stream_id      = ''
        self.app_id         = 0
        self.alias              = ''
        self.is_need_delay      = True
        self.potocol_ver        = 2

    def __str__(self):
        attr_str = 'alias:' + str(self.alias)
        attr_str += ' app_id:' + str(self.app_id)
        attr_str += ' stream_id:' + str(self.stream_id)
        attr_str += ' is_need_delay:' + str(self.is_need_delay)
        attr_str += ' potocol_ver:' + str(self.potocol_ver)
        return attr_str


class ServerStreamInfo:
    def __init__(self):
        self.stream_id = 0
        self.mark_time = int(time.time())
        self.block_seq = UNKNOWN_BLOCK_SEQ
        
class TimerNotify:
    def handle_timer(self):
        pass

    def handle_timeout(self):
        pass

class IOHandler:
    def handle_io(self, evt):
        pass

class IReactor:
    def add_fd(self, fd, io_handler, event=0):
        pass
    def remove_fd(self, fd, io_handler):
        pass
    def set_fd_event(self, fd, io_handler, event):
        pass
    def register_timer(self, timer_handler):
        pass
    def remove_timer(self, timer_handler):
        pass
    def svc(self, timeout):
        pass


