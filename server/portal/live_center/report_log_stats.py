#coding:utf-8 
import time
import logging
import socket
from stream_id_helper import *
from report_critical_info import * 
from stream_change_notify import *

class ReportLogStat(IStreamChangeNotify):
    def __init__(self, ip_id, port_id, rpt_pipe = '/tmp/rpt.pip'):
        self.ip_id = ip_id
        self.port_id = port_id
        self.rpt = ReportCriticalInfo(rpt_pipe)
        self.rpt.create(False)

    def __format_data(self, data):
        ts = time.localtime()
        ts = time.strftime('%Y-%m-%d %H:%M:%S', ts)
        ret = "%s:%s\tRPT\tlctrl\t%s\t%s\n" % (self.ip_id, str(self.port_id), ts, data)
        return  ret
       
    def __send_data(self, data):
        try:
            content = self.__format_data(data)
            self.rpt.send(content)
        except Exception as e:
            logging.warning('report msg:%s failed! Exception:%s', data, str(e))
            return

    def on_stream_create(self, si, do_create, is_hd):
        if not do_create:
            return

        fd = None
        try:
            helper = StreamIDHelper(si.stream_id)
            #(hd_id, sd_id, smooth_id) = helper.encode(si.stream_id)
            (hd_id, sd_id, smooth_id) = helper.encode_all()
            data = "STREAM_INFO_CREATE\t2.0\t%s %s %s %s\n" % (str(si.stream_id), str(si.app_id), str(si.upload_client_type), str(si.alias))
            self.__send_data(data)

            if is_hd:
                data = "STREAM_INFO_CREATE\t2.0\t%s %s %s %s\n" % (hd_id, str(si.app_id), str(si.upload_client_type), str(si.alias))
                self.__send_data(data)
                data = "STREAM_INFO_CREATE\t2.0\t%s %s %s %s\n" % (sd_id, str(si.app_id), str(si.upload_client_type), str(si.alias))
                self.__send_data(data)
                data = "STREAM_INFO_CREATE\t2.0\t%s %s %s %s\n" % (smooth_id, str(si.app_id), str(si.upload_client_type), str(si.alias))
                self.__send_data(data)

        except Exception as e:
            logging.exception("ReportLogStat::on_stream_create exception:" + str(e))
        finally:
            if None != fd:
                fd.close()
                fd = None
        

    def on_stream_destroy_notify(self, si, do_destroy):
        self.on_stream_destroy(si, do_destroy)

    def on_stream_destroy(self, si, do_destroy):
        if not do_destroy:
            return

        fd = None
        try:
            data = "STREAM_INFO_DESTROY\t2.0\t%s %s %s %s" % (str(si.stream_id), str(si.app_id), str(si.upload_client_type), str(si.alias))
            self.__send_data(data)
        except socket.error as e:
            logging.exception("ReportLogStat::on_stream_destroy find socket exception." + str(e))
        finally:
            if None != fd:
                fd.close()
                fd = None

