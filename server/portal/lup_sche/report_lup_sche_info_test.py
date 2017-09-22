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

    def __del__(self):
        if self.__sender != None:
            self.__sender.close()
        self.__sender = None
        
    def get_msg_head(self, msg_type = 'RPT'):
        ts = time.localtime()
        ts_str = time.strftime('%Y-%m-%d %H:%M:%S', ts)
        ret = "%s:%s\t%s\t%s\t%s" % (self.__ip, str(self.__port), msg_type, self.__server_name, ts_str)
        return ret

    def send(self, msg, msg_type = 'RPT'):
        if self.__sender == None:
            logging.warning('ReportLupscheInfo::send __sender = None,Report failed!')
            return
        new_msg = self.get_msg_head(msg_type) + msg
        self.__sender.send(new_msg)

    def send_orgion(self, msg):
        if self.__sender == None:
            logging.warning('ReportLupscheInfo::send __sender = None,Report failed!')
            return
        self.__sender.send(msg)

    def send_v1(self, stream_id, user_id, chn_id, receiver_addr_ex,receiver_addr, ants_addr, rtmp_addr):
        #format: \tchn_id\tstream_id\tuser_id\treceiver_addr\tants_addr\trtmp_addr\n
        new_msg = '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' % ( self.get_msg_head(),str(chn_id), str(stream_id), str(user_id), 
                 str(receiver_addr_ex), str(receiver_addr), str(ants_addr), str(rtmp_addr))
        logging.warning("rpt send_v1: new_msg:%s",new_msg)
        self.__sender.send(new_msg)

    def send_v2(self, sche_info):
        if sche_info == None:
            logging.warning('send_v2: send failed! sche_info = None')
            return
        for streamid in sche_info.stream_list:
            self.send_v1(streamid, sche_info.user_id, sche_info.chn_id, sche_info.receiver_ex_addr,
                    sche_info.receiver, sche_info.ants_addr, sche_info.rtmpserver_addr)
        sche_info.clear()


if __name__ == "__main__": 
    rpt = ReportLupscheInfo("10.10.69.195","80","/tmp/rpt.pip")
    #rpt.create(False)
    #rpt.recv(callback_recv_msg)
    while True:
        #rpt.send_orgion('msg_start_pos 10.10.69.195:8000   RPT     lup_sche        2014-11-24 11:44:10     24913   5341    10.10.69.195        10.10.69.121\n')
        rpt.send_v1('555##%^$$#$$@&#2356552aef33','2333256677','4432','','10.10.69.120','10.10.69.120','')
        time.sleep(10)
    rpt.close()



