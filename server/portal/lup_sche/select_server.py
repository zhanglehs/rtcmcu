# -*- coding:utf-8 -*-
import logging
from sche_algorith import *
from lup_sche_xml import *
from sort_server import *

class SelectServer(object):
    """description of class"""
    def __init__(self, conf_info = None):
        self.__conf_info = conf_info
        self.__sche_algorith = ScheAlgorith()

        if None != conf_info:
            self.set_conf_info(self.__conf_info)

    def set_conf_info(self, conf_info):
        try:
            self.__conf_info = conf_info
            sort = SortServer()
            self.__conf_info.receiver_list = sort.sort_addr_list_v3(self.__conf_info.receiver_list, 'ip', 'uploader_port')
            self.__conf_info.receiver_ex_list = sort.sort_addr_list_v3(self.__conf_info.receiver_ex_list, 'ip', 'uploader_port', 'player_port')
            self.__conf_info.ants_list = sort.sort_addr_list_v3(self.__conf_info.ants_list)
            self.__conf_info.rtmp_server_list = sort.sort_addr_list_v3(self.__conf_info.rtmp_server_list)
            return True
        except Exception as e:
            print('SelectServer::set_conf_info Exception e:%s' % str(e))
        return False

    def get_server(self, server_list, dst_ip = None, stream_id = None):
        server = None
        if None != dst_ip:
            server = self.__sche_algorith.get_local_server(server_list, dst_ip)
        elif None == stream_id:
            server = self.__sche_algorith.get_server_hash(server_list, 0)
        else:
            server = self.__sche_algorith.get_server_hash(server_list, stream_id)
        return server

    def get_receiver(self, dst_ip = None, stream_id = None, is_rtp = False):
        ret_val = []
        if len(self.__conf_info.receiver_list) <= 0:
            return ret_val
        if not is_rtp:
            server = self.get_server(self.__conf_info.receiver_list, dst_ip, stream_id)
        else:
            server = self.get_server(self.__conf_info.receiver_rtp_list, dst_ip, stream_id)

        ret_val.append(server['ip'])
        ret_val.append(int(server['uploader_port']))
        ret_val.append(int(server['sdp_port']))
        return ret_val

    def get_receiver_ex(self, dst_ip = None, stream_id = None, is_rtp = False):
        ret_val = []
        if len(self.__conf_info.receiver_ex_list) <= 0:
            return ret_val
        if not is_rtp:
            server = self.get_server(self.__conf_info.receiver_ex_list, dst_ip, stream_id)
        else:
            server = self.get_server(self.__conf_info.receiver_ex_rtp_list, dst_ip, stream_id)

        logging.info('SelectServer::get_receiver_ex server:%s, receiver_ex_list:%s stream_id:%s',str(server), str(self.__conf_info.receiver_ex_list), str(stream_id))
        ret_val.append(server['ip'])
        ret_val.append(int(server['uploader_port']))
        ret_val.append(int(server['player_port']))
        ret_val.append(int(server['sdp_port']))
        return ret_val

    def get_ants(self, dst_ip = None, stream_id = None):
        ret_val = []
        if len(self.__conf_info.ants_list) <= 0:
            return ret_val
        #server = self.get_server(self.__conf_info.ants_list, dst_ip, stream_id)
        server = self.__sche_algorith.get_server_hash_by_cnt(self.__conf_info.ants_list)
        ret_val.append(server['ip'])
        ret_val.append(int(server['port']))
        return ret_val

    def get_rtmp_server_ex(self, dst_ip = None, stream_id = None):
        ret_val = []
        if len(self.__conf_info.rtmp_server_list) <= 0:
            return ret_val
        server = self.get_server(self.__conf_info.rtmp_server_list, dst_ip, stream_id)
        
        ret_val.append(server['ip'])
        ret_val.append(int(server['port']))
        return ret_val

'''
if __name__ == "__main__":
    select_server = SelectServer()
    file_name = 'D:\\work\\python\\xml\\test\\lup_sche.xml'
    conf = LupScheXML(file_name, select_server.set_conf_info)
    conf.load()
    print('select_server: %s' % ( str(select_server)))
'''

