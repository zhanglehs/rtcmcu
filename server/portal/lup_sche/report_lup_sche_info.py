import os
import time
import logging
from report_critical_info import ReportCriticalInfo 

class ScheInfo:
    def __init__(self):
        self.receiver_ex_addr = None
        self.receiver_addr = None
        self.ants_addr = None
        self.rtmpserver_addr = None
        self.streamid_list = []
        self.user_id = None
        self.chn_id = None

    def clear(self):
        self.receiver_ex_addr = None
        self.receiver_addr = None
        self.ants_addr = None
        self.rtmpserver_addr = None
        del self.streamid_list[:]
        self.user_id = None
        self.chn_id = None


class ReportLupscheInfo:
    def __init__(self, ip, port, pipName):
        self.__server_name = "lup_sche"
        self.__ip = ip
        self.__port = port
        self.__sender = ReportCriticalInfo(pipName)
        self.__sender.create(False)
        logging.warning('ReportLupscheInfo __init__(%s,%s,%s)', str(ip), str(port), str(pipName))
 
    
    def close(self):
        if self.__sender != None:
            self.__sender.close()
        self.__sender = None   

    def get_msg_head(self, msg_type = 'RPT'):
        ts = time.localtime()
        ts_str = time.strftime('%Y-%m-%d %H:%M:%S', ts)
        ret = "%s:%s\t%s\t%s\t%s" % (self.__ip, str(self.__port), msg_type, self.__server_name, ts_str)
        return ret

    def send_origion(self, msg):
        if self.__sender == None:
            logging.warning('ReportLupscheInfo::send_origion __sender = None,Report failed!')
            return
        self.__sender.send(msg)


    def send(self, msg, msg_type = 'RPT'):
        if self.__sender == None:
            logging.warning('ReportLupscheInfo::send __sender = None,Report failed!')
            return
        new_msg = self.get_msg_head(msg_type) + msg
        self.__sender.send(new_msg)

    def send_v1(self, stream_id, user_ip, chn_id, receiver_addr_ex,receiver_addr, ants_addr, rtmp_addr):
        #format: \tchn_id\tstream_id\tuser_id\treceiver_addr\tants_addr\trtmp_addr\n
        #max = (a if a > b else b)
        stream_id_str = ('0' if( (stream_id == None) or (stream_id=='')) else stream_id)
        chn_id_str = ('0' if( (chn_id == None) or (chn_id=='')) else chn_id)
        user_ip_str = ('0' if( (user_ip == None) or (user_ip =='')) else user_ip)
        receiver_ex_ip_str = ('0' if( (receiver_addr_ex == None) or (receiver_addr_ex =='')) else receiver_addr_ex)
        receiver_ip_str = ('0' if( (receiver_addr == None) or (receiver_addr =='')) else receiver_addr)
        ants_ip_str = ('0' if( (ants_addr == None) or (ants_addr =='')) else ants_addr)
        rtmp_ip_str = ('0' if( (rtmp_addr == None) or (rtmp_addr =='')) else rtmp_addr)
        new_msg = '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' % ( self.get_msg_head(), chn_id_str, stream_id_str, user_ip_str, 
                 receiver_ex_ip_str, receiver_ip_str, ants_ip_str, rtmp_ip_str)
        logging.warning("rpt send_v1: new_msg:%s",new_msg)
        self.__sender.send(new_msg)

    def send_v2(self, sche_info):
        if sche_info == None:
            logging.warning('send_v2: send failed! sche_info = None')
            return
        for streamid in sche_info.streamid_list:
            self.send_v1(streamid, sche_info.user_id, sche_info.chn_id, sche_info.receiver_ex_addr,
                    sche_info.receiver_addr, sche_info.ants_addr, sche_info.rtmpserver_addr)
        sche_info.clear()




