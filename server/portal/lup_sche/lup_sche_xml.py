# -*- coding:utf-8 -*-
from xml_parse import BaseParseXML
import logging

g_conf_info = None
g_lup_sche_xml = None

class ConfigInfo:
    def __init__(self):
        self.common_list = None
        self.receiver_list = None
        self.receiver_rtp_list = None
        self.receiver_ex_list = None
        self.receiver_ex_rtp_list = None
        self.rtmp_server_list = None
        self.ants_list = None
        self.bitrate_video = None
        self.up_sche_secret = None
        self.upload_secret = None
        self.listen = None
        self.log = None
        self.rpt = None

    def clear_dict(self, list):
        if None == list:
            return
        length = len(list)
        for i in range(0, length,1):
            i.clear()
        del list[:]

    def clear(self):
        self.clear_dict(self.common_list)
        self.clear_dict(self.receiver_list)
        self.clear_dict(self.receiver_rtp_list)
        self.clear_dict(self.receiver_ex_list)
        self.clear_dict(self.receiver_ex_rtp_list)
        self.clear_dict(self.rtmp_server_list)
        self.clear_dict(self.ants_list)            
        self.clear_dict(self.bitrate_video)
        self.clear_dict(self.up_sche_secret)
        self.clear_dict(self.upload_secret)
        self.clear_dict(self.listen)
        self.clear_dict(self.log)
        self.clear_dict(self.rpt)

    def dump(self):
        logging.info('common:%s' % str(self.common_list))
        logging.info('receiver_list:%s' % str(self.receiver_list))
        logging.info('receiver_rtp_list:%s' % str(self.receiver_rtp_list))
        logging.info('receiver_ex_list:%s' % str(self.receiver_ex_list))
        logging.info('receiver_ex_rtp_list:%s' % str(self.receiver_ex_rtp_list))
        logging.info('ants_list:%s' % str(self.ants_list))
        logging.info('rtmp_server_list:%s' % str(self.rtmp_server_list))
        logging.info('bitrate_video:%s' % str(self.bitrate_video))
        logging.info('up_sche_secret:%s' % str(self.up_sche_secret))
        logging.info('upload_secret:%s' % str(self.upload_secret))
        logging.info('listen:%s' % str(self.listen))
        logging.info('log:%s' % str(self.log))
        logging.info('rpt:%s' % str(self.rpt))



class LupScheXML(BaseParseXML):

    @staticmethod
    def create(file_name, callback = None):
        global g_conf_info
        global g_lup_sche_xml
        g_lup_sche_xml = LupScheXML(file_name, callback)
        if g_lup_sche_xml.load():
            g_conf_info = g_lup_sche_xml.conf_info
            return True
        return False

    @staticmethod
    def update(conf_info):
        global g_conf_info
        if None != g_conf_info:
            g_conf_info.clear()
        g_conf_info = conf_info


    def __init__(self, file_name, callback = None):
        BaseParseXML.__init__(self, file_name)
        self.conf_info = ConfigInfo()
        self.callback = callback

    def __load(self):
        self.conf_info.clear()
        self.conf_info.common_list = self.handle_catalog_attr_list('common')
        self.conf_info.receiver_list = self.handle_catalog_attr_list('receiver_list')
        self.conf_info.receiver_rtp_list = self.handle_catalog_attr_list('receiver_rtp_list')
        self.conf_info.receiver_ex_list = self.handle_catalog_attr_list('receiver_ex_list')
        self.conf_info.receiver_ex_rtp_list = self.handle_catalog_attr_list('receiver_ex_rtp_list')
        self.conf_info.ants_list = self.handle_catalog_attr_list('ants_list')
        self.conf_info.rtmp_server_list = self.handle_catalog_attr_list('rtmp_server_list')
        self.conf_info.bitrate_video = self.handle_catalog_attr_list('bitrate_video')[0]
        self.conf_info.up_sche_secret = self.handle_catalog_attr_list('up_sche_secret')[0]
        self.conf_info.upload_secret = self.handle_catalog_attr_list('upload_secret')[0]
        self.conf_info.listen = self.handle_catalog_attr_list('listen')[0]
        self.conf_info.log = self.handle_catalog_attr_list('log')[0]
        self.conf_info.rpt = self.handle_catalog_attr_list('rpt')[0]

    def load(self):
        try:
            self.__load()
        except Exception as e:
            print('LupScheXML::load Exception e:%s' % str(e))
        finally:
            if None != self.callback:
                self.callback(self.conf_info)
            return True
        return False

    def get_conf_info():
        return self.conf_info
        

   
