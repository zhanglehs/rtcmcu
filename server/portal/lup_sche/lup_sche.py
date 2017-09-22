import lproto
import logging
import struct
import os
import time
import sys

def cur_file_parent_dir():
    return os.path.dirname(cur_file_dir())
 
def cur_file_dir():
    path = os.path.realpath(sys.path[0]) 
    if os.path.isdir(path):
        return path
    elif os.path.isfile(path):
        return os.path.dirname(path)

try:
    sys.path.append(cur_file_parent_dir() + '/common')
    sys.path.append(cur_file_dir() + '/common')
except:
    pass

try:
    import hashlib
except ImportError:
     # for Python << 2.5
    import md5
 
import lmain_base
from lmain_base import lmain_base_t
from lhttp_base import lhttp_base_t, lhttp_base_handler_t
from lbin_base import lbin_base_t, lbin_base_handler_t

   
import socket
import lup_notify
import lhttp_base 
import const
import lutil
import json
from lup_sche_v3 import *
from access_log import AccessLog
from sche_algorith import ScheAlgorith
import lup_sche_xml
from sort_server import *
from select_server import *
from report_lup_sche_info import ReportLupscheInfo 


class my_sche_t:
    def __init__(self, select_svr):
        self.__select_svr = select_svr

    def get_one_addr(self, stream_id, host = None, is_rtp = False):
        logging.info('get_one_addr start')
        svr = self.__select_svr.get_receiver(host, stream_id, is_rtp)
        logging.info('get_one_addr:%s',str(svr))
        if len(svr) <= 0:
            logging.warning('receiver ip list is empty')
            return ('', 0, 0)

        #random.seed(stream_id)
        #addr = random.choice(self.addr_list)
        logging.debug("get_one_addr return %s for stream_id %d", str(svr), stream_id)
        return svr

    def get_rtmp_addr(self, stream_id, host = None):
        svr = self.__select_svr.get_rtmp_server_ex(host, stream_id)
        if len(svr) <= 0:
            logging.warning('rtmpserver ip list is empty')
            return ('', 0)

        logging.debug("get_rtmp_addr return %s for stream_id %d", str(svr), stream_id)
        return svr

    def get_notify_addr(self, stream_id, host = None):
        svr = self.__select_svr.get_ants(host, stream_id)
        if len(svr) <= 0:
            logging.warning('ants ip list is empty')
            return ('', 0)

        logging.debug("get_notify_addr return %s for stream_id %d", str(svr), stream_id)
        return svr

    def get_receiver_ex_addr(self, stream_id, host = None, is_rtp = False):
        logging.info('lup_sche::get_receiver_ex_addr start, stream_id:%s', str(stream_id))
        svr = self.__select_svr.get_receiver_ex(host, stream_id, is_rtp)
        logging.info('lup_sche::get_receiver_ex_addr :%s', str(svr))
        if len(svr) <= 0:
            logging.warning('lup_sche::receiver_ex_addr ip list is empty')
            return ('', 0, 0, 0)

        logging.info('lup_sche::get_receiver_ex_addr, result:%s', str(svr))

        return svr[0], svr[1], svr[2], svr[3]

class my_http_t(lhttp_base_t):
    def __init__(self, rsvc, msvc, ip, port, report_handler):
        lhttp_base_t.__init__(self, rsvc, ip, port)
        self.msvc = msvc
        self.__common_handler = None
        self.__report_handler = report_handler

    def attach(self, common_handler):
        self.__common_handler = common_handler

    def detach(self):
        self.__common_handler = None

    def on_up_sche_ex(self, handler, para_m, client_addr, user_id_str, room_id_str, sche_token ,stream_id_str, out_handler = None):
        if ('' == user_id_str or '' == room_id_str or
                '' == sche_token or '' == stream_id_str):
            logging.info("find invalid para using HTTP")
            return 400

        stream_id = 0
        try:
            stream_id = int(stream_id_str)
            if stream_id <= 0:
                logging.debug("find invalid stream_id %s using HTTP",
                              stream_id_str)
                return 400
        except:
            logging.debug("find invalid stream_id %s using HTTP",
                          stream_id_str)
            return 400

        curr_time = int(time.time())
        result = lutil.check_token(curr_time, stream_id,
                user_id_str, '',
                lup_sche_xml.g_conf_info.up_sche_secret['private_key'],
                int(lup_sche_xml.g_conf_info.up_sche_secret['token_timeout']), sche_token)
        if not result and sche_token != lup_sche_xml.g_conf_info.upload_secret['backdoor_token']:
            logging.warning("can't check sche_token %s for user_id(%s) and stream(%d) using HTTP",
                    sche_token, user_id_str, stream_id)
            return 403

        (ip_str, port, sdp_port) = self.msvc.ssvc.get_one_addr(stream_id)
        if '' == ip_str:
            logging.warning("can't get any ip for user_id(%s) and stream(%d) using HTTP",
                            user_id_str, stream_id)
            return 500

        extra = '%s:%d' % (ip_str, port)
        up_token = lutil.make_token(curr_time, stream_id, user_id_str, extra,
                                    lup_sche_xml.g_conf_info.upload_secret['private_key'])

        logging.info("return %s:%d for user_ip(%s) user_port(%s) user_id(%s) and stream(%d) using http",
                ip_str, port, str(client_addr[0]), str(client_addr[1]), user_id_str, stream_id)

        if out_handler != None:
            out_handler.sche_result(handler,ip_str,port,user_id_str, up_token, stream_id, 200)
            self.__report_handler.send_v1(str(stream_id), str(client_addr[0]), str(room_id_str), "", ip_str, "", "")
            return 0

        ret_url = 'http://%s:%d/v1/r?uid=%s&a=%s&b=%x' % (ip_str, port, user_id_str, up_token, stream_id)
        eh = 'Location: %s\r\n' % (ret_url)

        handler.send_rsp("", 302, extra_header = eh)
        
        self.__report_handler.send_v1(str(stream_id), str(client_addr[0]), str(room_id_str), "", ip_str, "", "")
        
        return 0


    def encode_rtmp_upload_url_with_send(self, handler, stream_id, error_code_str, rtmp_url_pub):
        ret_dict = {"stream_id": str(stream_id), "error_code": error_code_str, "rtmp_url":rtmp_url_pub }
        json.JSONEncoder().encode(ret_dict)
        handler.send_rsp(json.JSONEncoder().encode(ret_dict))


    def make_rtmp_upload_url_with_send(self, handler, stream_id, user_id_str, rtmp_app_str):
        curr_time = int(time.time())
        (rtmpserver_ip_str, rtmpserver_port) = self.msvc.ssvc.get_rtmp_addr(stream_id)
        rtmp_url_pub,rtmp_url_play = self.make_rtmp_upload_url(curr_time, rtmpserver_ip_str, rtmpserver_port, stream_id, user_id_str, rtmp_app_str)

        self.encode_rtmp_upload_url_with_send(handler, stream_id, "0", rtmp_url_pub)

        return (0, [rtmpserver_ip_str,rtmpserver_port], rtmp_url_pub, rtmp_url_play)

    def make_rtmp_upload_url(self, curr_time, rtmpserver_ip_str, rtmpserver_port, stream_id, user_id_str, rtmp_app_str):
        extra = '%s:%d' % (rtmpserver_ip_str, rtmpserver_port)
        up_token = lutil.make_token(curr_time, stream_id, user_id_str, extra,
                                    lup_sche_xml.g_conf_info.upload_secret['private_key'])

        key = "xingmeng.com" + str(curr_time)
        logging.warning("key: %s", key)
        try: 
            value = hashlib.md5(key.encode('utf8')).hexdigest()
        except:
            value = md5.new(key).hexdigest()
    
        rtmp_url_play = 'rtmp://%s:%d/%s/%x' %(rtmpserver_ip_str, rtmpserver_port, rtmp_app_str, int(stream_id))
        rtmp_url_pub = '%s?v=1&k1=%s&k2=%s' % (rtmp_url_play, value[:5], str(curr_time))

        return (rtmp_url_pub, rtmp_url_play)
 

    def on_up_rtmp_sche_ex(self, handler, para_m, client_addr, user_id_str, room_id_str, sche_token, stream_id_str, rtmp_app_str):
        (ecode, crtmpserver_addr, receiver_addr) = self.on_up_rtmp_sche_v2(handler, para_m, client_addr, user_id_str, room_id_str, 
                sche_token, stream_id_str, rtmp_app_str)
        return ecode

    def on_up_rtmp_sche_v2(self, handler, para_m, client_addr, user_id_str, room_id_str, sche_token, stream_id_str, rtmp_app_str):
        if ('' == user_id_str or '' == room_id_str or
                '' == sche_token or '' == stream_id_str):
            logging.info("find invalid para using HTTP")
            return (400, None, None)

        stream_id = 0
        try:
            stream_id = int(stream_id_str)
            if stream_id <= 0:
                logging.debug("find invalid stream_id %s using HTTP",
                              stream_id_str)
                return (400, None, None)
        except:
            logging.debug("find invalid stream_id %s using HTTP",
                          stream_id_str)
            return (402, None, None)

        curr_time = int(time.time())
        result = lutil.check_token(curr_time, stream_id,
                user_id_str, '',
                lup_sche_xml.g_conf_info.up_sche_secret['private_key'],
                int(lup_sche_xml.g_conf_info.up_sche_secret['token_timeout']),  
                sche_token)
        if not result and sche_token != lup_sche_xml.g_conf_info.upload_secret['backdoor_token']:
            logging.warning("can't check sche_token %s for user_id(%s) and stream(%d) using HTTP",
                    sche_token, user_id_str, stream_id)
            return (403, None, None)

        (ip_str, port) = self.msvc.ssvc.get_rtmp_addr(stream_id)
        (ip_receiver_str, receiver_port, sdp_port) = self.msvc.ssvc.get_one_addr(stream_id)
        if '' == ip_str:
            logging.warning("can't get any ip for user_id(%s) and stream(%d) using HTTP",
                            user_id_str, stream_id)
            return (500, None, None)

        extra = '%s:%d' % (ip_str, port)
        up_token = lutil.make_token(curr_time, stream_id, user_id_str, extra,
                                    lup_sche_xml.g_conf_info.upload_secret['private_key'])

        key = "xingmeng.com" + str(curr_time)
        logging.warning("key: %s", key)
        try: 
            value = hashlib.md5(key.encode('utf8')).hexdigest()
        except:
            value = md5.new(key).hexdigest()
    
        logging.warning("value: %s", value)
        rtmp_url = 'rtmp://%s:%d/%s/%x?v=1&k1=%s&k2=%s' %(ip_str, port, rtmp_app_str, stream_id, value[:5], str(curr_time))
        logging.warning("return upload addr: %s for user_ip(%s) user_port(%s) user_id(%s) and stream(%d) using http",
                rtmp_url, str(client_addr[0]), str(client_addr[1]), user_id_str, stream_id)


        source_format = int(para_m.get('f', ['0'])[0].strip())
        if source_format == 0:
            handler.send_rsp(rtmp_url)
        else:
            self.encode_rtmp_upload_url_with_send(handler, stream_id, "0", rtmp_url)

        (ip_notify_str, notify_port) = self.msvc.ssvc.get_notify_addr(stream_id)

        rtmp_url = rtmp_url[:rtmp_url.find('?')]
        lup_notify.notify_service_start(ip_notify_str, notify_port, rtmp_url, "rtmp", stream_id, ip_receiver_str, receiver_port, "flv")
        
        self.__report_handler.send_v1(str(stream_id), str(client_addr[0]), str(room_id_str), "", ip_receiver_str, ip_notify_str, ip_str)

        return (0,[ip_notify_str, notify_port], [ip_receiver_str, receiver_port])

    def on_up_sche(self, handler, para_m, client_addr):
        user_id_str = para_m.get('uid', [''])[0].strip()
        room_id_str = para_m.get('rid', [''])[0].strip()
        sche_token = para_m.get('a', [''])[0].strip()
        stream_id_str = para_m.get('b', [''])[0].strip()
        return self.on_up_sche_ex(handler, para_m, client_addr, user_id_str, room_id_str, sche_token, stream_id_str)

    def on_up_rtmp_sche(self, handler, para_m, client_addr):
        user_id_str = para_m.get('uid', [''])[0].strip()
        room_id_str = para_m.get('rid', [''])[0].strip()
        sche_token = para_m.get('a', [''])[0].strip()
        stream_id_str = para_m.get('b', [''])[0].strip()
        rtmp_app_str = para_m.get('c', [''])[0].strip()
        logging.info("rtmp_app_str : %s" , rtmp_app_str)
        return self.on_up_rtmp_sche_ex(handler, para_m, client_addr, user_id_str, room_id_str, sche_token, stream_id_str, rtmp_app_str)
     
    #supported flv and rtmp
    #format: http://ip:port/v3/us?uid=xxxx&rid=xxxx&a=xxx&b=xxx&c=xxx&r=xxx&f=xxxÂ 
    def on_up_sche_v3(self, handler, para_m, client_addr):
        if None != self.__common_handler:
            return self.__common_handler.on_sche_for_http(handler, para_m, client_addr)
        return 500

    #supported flv and rtmp
    #format: http://ip:port/v1/us?a=xxx&b=xxx&r=xxx&et=xxx&dur=xxxÂ 
    def on_up_sche_for_net_stream(self, handler, para_m, client_addr):
        if None != self.__common_handler:
            return self.__common_handler.on_sche_for_net_stream(handler, para_m, client_addr)
        return 500



    def on_notify_server(self, handler, para_m):
        src_url = para_m.get('src_url', [''])[0].strip()
        src_type = para_m.get('src_type', [''])[0].strip()
        stream_id_str = para_m.get('stream_id', [''])[0].strip()
        action = para_m.get('action', [''])[0].strip()
        b_v = para_m.get('b_v', [''])[0].strip()
        b_a = para_m.get('b_a', [''])[0].strip()
        size = para_m.get('size', [''])[0].strip()
        try:
            stream_id = int(stream_id_str)
            if stream_id <= 0:
                logging.debug("find invalid stream_id %s using HTTP", stream_id_str)
                return 400
            (ip_receiver_str, receiver_port, sdp_port) = self.msvc.ssvc.get_one_addr(stream_id)
            (ip_notify_str, notify_port) = self.msvc.ssvc.get_notify_addr(stream_id)
        except:
            logging.debug("find invalid stream_id %s using HTTP", stream_id_str)
            return 402

        if 'stop' == action and stream_id != '' :
            result = lup_notify.notify_service_stop(ip_notify_str, notify_port, stream_id)
            logging.info("action stop stream_id = %s", stream_id)
            handler.send_rsp(lhttp_base.result_json(result))
            return 0

        if 'start' != action:
            logging.debug("find invalid action = %", action)
            return 400
        
        if '' == src_url or '' == src_type or '' == stream_id:
            logging.warning("notify argument missing: src_url = %s, src_type = %s, stream_id = %s", src_url, src_type, stream_id)
            return 500

        if '' == ip_receiver_str:
            logging.warning("can't get any ip for user_id(%s) and stream(%d) using HTTP",
                            user_id_str, stream_id)
            return 500
        
        result = lup_notify.notify_service_start(ip_notify_str, notify_port, src_url, src_type, stream_id, ip_receiver_str, receiver_port, "flv", b_v, b_a, size)
        handler.send_rsp(lhttp_base.result_json(result))
        return 0

    def on_crossdomain(self, handler, para_m):
        cross_domain_content = '<?xml version="1.0" encoding="UTF-8"?>\
<cross-domain-policy>\
<allow-access-from domain="*.youku.com" secure="true"/>\
<allow-access-from domain="*.yodou.com" secure="true"/>\
<allow-access-from domain="*.xingmeng.com" secure="true"/>\
<allow-access-from domain="xingmeng.com" secure="true"/>\
<allow-access-from domain="*" secure="true"/>\
</cross-domain-policy>'
        handler.send_rsp(cross_domain_content)
        return 0

    def on_handler_die(self, h):
        lhttp_base_t.on_handler_die(self, h)

    def on_post(self, handler, path, para_m, client_addr):
        return self.msg_proc(handler, path, para_m, client_addr)
    
    def on_get(self, handler, path, para_m, client_addr):
        return self.msg_proc(handler, path, para_m, client_addr)

    def msg_proc(self, handler, path, para_m, client_addr):
        try:
            if path == "/v1/us":
                return self.on_up_sche(handler, para_m, client_addr)

            elif path == "/v2/us":
                return self.on_up_rtmp_sche(handler, para_m, client_addr)

            elif path == "/v3/us":
                return self.on_up_sche_v3(handler, para_m, client_addr)

            elif path == "/v1/nus":
                return self.on_up_sche_for_net_stream(handler, para_m, client_addr)

            elif path == "/notify/us": # called by web backend (yongning)
                return self.on_notify_server(handler, para_m)

            elif path == "/crossdomain.xml":
                return self.on_crossdomain(handler, para_m)

        except Exception as e:
	        logging.info('on_get path:%s para_m:%s e:%s', path, para_m, e)
        return 404

class my_handler_t(lbin_base_handler_t):
    access_log = None
    @staticmethod
    def create_handler(rsvc, ssvc, h_para, common_handler):
        return my_handler_t(rsvc, ssvc, h_para, common_handler)

    def __init__(self, rsvc, ssvc, handler_para, common_handler):
        lbin_base_handler_t.__init__(self, rsvc, handler_para, common_handler)
        self.ssvc = ssvc

    def on_req_addr_impl(self, pkg):
        curr_time = int(time.time())
        stream_id = pkg["streamid"]
        room_id = pkg['roomid']
        user_id_str = str(pkg["userid"])
        sche_token = pkg["sche_token"].rstrip('\x00')
        result = lutil.check_token(curr_time, stream_id, user_id_str, '',
                                lup_sche_xml.g_conf_info.up_sche_secret['private_key'], 
                                int(lup_sche_xml.g_conf_info.up_sche_secret['token_timeout']),
                                sche_token)
        if not result and sche_token != lup_sche_xml.g_conf_info.upload_secret['backdoor_token']:
            logging.warning("can't check sche_token %s for user_id(%s) and stream(%d)",
                            sche_token, user_id_str, stream_id)
            return (1, 0, 0, '')

        ip = 0
        port = 0

        old_ip = pkg["ip"]
        old_port = pkg["port"]
        if 0 != old_ip and 0 != old_port:
            up_token = pkg["up_token"].rstrip('\x00')
            extra = '%s:%d' % (lutil.ip2str(old_ip), old_port)
            result = lutil.check_token(curr_time, stream_id, user_id_str, extra,
                                    lup_sche_xml.g_conf_info.upload_secret['private_key'],
                                    int(lup_sche_xml.g_conf_info.upload_secret['token_timeout']),
                                    up_token)
            if not result and sche_token != lup_sche_xml.g_conf_info.upload_secret['backdoor_token']:
                logging.warning("can't check up_token %s for user_id(%s) stream(%d) extra(%s)",
                                up_token, user_id_str, stream_id, extra)
                return (1, 0, 0, '')

            ip = old_ip
            port = old_port
        else:
            (ip_str, port, sdp_port) = self.ssvc.get_one_addr(stream_id)
            if '' != ip_str:
                ip = lutil.ipstr2ip(ip_str)

        if 0 == ip:
            logging.warning("can't get any ip for user_id(%s) and stream(%d)",
                            user_id_str, stream_id)
            return (1, 0, 0, '')

        extra = '%s:%d' % (lutil.ip2str(ip), port)
        up_token = lutil.make_token(curr_time, stream_id, user_id_str, extra,
                                    lup_sche_xml.g_conf_info.upload_secret['private_key'])
        client_addr = lbin_base_handler_t.get_client_addr(self)
        
        try:
            logging.info("return %s:%d for old_ip(%s) old_port(%s) user_ip(%s) user_port(%s) user_id(%s) and stream(%d)",
                         lutil.ip2str(ip), port, lutil.ip2str(old_ip), str(old_port), str(client_addr[0]), str(client_addr[1]), user_id_str, stream_id)
            if self.access_log != None:
               first_line = 'uid=%s&rid=%s&sid=%s&token=%s rsp:%s:%s&token=%s' % ( user_id_str, str(room_id), str(stream_id), sche_token, lutil.ip2str(ip), str(port), str(up_token))
               self.access_log.add_v3(first_line, client_addr)
        except Exception as e:
            logging.info('on_req_addr_impl Exception:%s', str(e))

        return (0, ip, port, up_token)


    def on_req_addr(self, pkg):
        pkg_d = {}
        try:
            logging.info("on_req_addr for bin. start")
            ret = struct.unpack(lproto.u2us_req_addr_pat, pkg)
            pkg_d["version"] = ret[0]
            pkg_d["uploader_code"] = ret[1]
            pkg_d["roomid"] = ret[2]
            pkg_d["streamid"] = ret[3]
            pkg_d["userid"] = ret[4]
            pkg_d["sche_token"] = ret[5].rstrip('\x00')
            pkg_d["up_token"] = ret[6].rstrip('\x00')
            pkg_d["ip"] = ret[7]
            pkg_d["port"] = ret[8]
            pkg_d["stream_type"] = ret[9]
        except struct.error:
            logging.debug("can't process pkg")
            return
        stream_id = pkg_d["streamid"]

        (reason, ip, port, up_token) = self.on_req_addr_impl(pkg_d)

        try:
            pkg = struct.pack(lproto.us2u_rsp_addr_pat, stream_id, ip, port,
                              reason, up_token)
            pkg = lproto.gen_pkg(lproto.us2u_rsp_addr_cmd,
                                 pkg)
        except struct.error:
            logging.info("can't encode pkg")
            return

        self.send_pkg(pkg)

    #supported v3.1 
    def on_req_addr_v2(self, pkg):
        pkg_d = {}
        try:
            ret = struct.unpack(lproto.u2us_req_addr_v2_pat, pkg)
            pkg_d["roomid"] = ret[0]
            pkg_d["streamid"] = ret[1]
            pkg_d["userid"] = ret[2]
            pkg_d["sche_token"] = ret[3].rstrip('\x00')
            pkg_d["stream_type"] = ret[4]
            pkg_d["width"] = ret[5]
            pkg_d["height"] = ret[6]
        except struct.error:
            logging.debug("on_req_addr_v2 can't process pkg")
            return
     
        (reason, ip, port, up_token) = self.__common_handler.on_sche_for_bin(pkg_d, lbin_base_handler_t.get_client_addr)
        try:
            stream_id = pkg_d["streamid"]
            pkg = struct.pack(lproto.us2u_rsp_addr_v2_pat, stream_id, ip, port, reason, up_token)
            pkg = lproto.gen_pkg(lproto.us2u_rsp_addr_v2_cmd, pkg)
        except struct.error:
            logging.info("on_req_addr_v2 can't encode pkg")
            return
        self.send_pkg(pkg)

    def on_pkg_arrive(self, cmd, pkg):
        if lproto.u2us_req_addr_cmd == cmd:
            self.on_req_addr(pkg)
        if lproto.u2us_req_addr_v2_cmd == cmd:
            self.on_req_addr_v2(pkg)
        else:
            logging.info("unknown cmd %d, pkg size %d",cmd, len(pkg))

    def on_handler_die(self):
        # do nothing
        pass


class ctrl_handler_t(lbin_base_handler_t):
    @staticmethod
    def create_handler(rsvc, ssvc, h_para):
        return ctrl_handler_t(rsvc, ssvc, h_para)

    def __init__(self, rsvc, ssvc, handler_para):
        lbin_base_handler_t.__init__(self, rsvc, handler_para)
        self.ssvc = ssvc

    def req_upload(self, seq_id, stream_id, user_id, token):
        pkg = ''
        try:
            pkg = struct.pack(lproto.us2r_req_up_pat, seq_id, stream_id, user_id, token)
        except struct.error:
            return False

        pkg = lproto.gen_pkg(lproto.us2r_req_up_cmd, pkg)
        self.send_pkg(pkg)
        return True

    def on_rsp_up(self, pkg):
        seq_id = 0
        stream_id = 0
        result = 0
        try:
            (seq_id, stream_id, result) = struct.unpack(lproto.r2us_rsp_up_pat, pkg)
        except struct.error:
            return
        self.ssvc.on_receiver_ack(seq_id, stream_id, result)

    def on_keepalive(self, pkg):
        # TODO
        self.ssvc.on_keepalive()

    def on_pkg_arrive(self, cmd, pkg):
        if lproto.r2us_rsp_up_cmd == cmd:
            self.on_rsp_up(pkg)
        elif lproto.r2us_keepalive_pat == cmd:
            self.on_keepalive(pkg)
        else:
            logging.info("unknown cmd %d, pkg size %d",
                         cmd, len(pkg))

    def on_handler_die(self):
        # do nothing
        pass

class my_bin_t(lbin_base_t):
    def __init__(self, rsvc, ssvc, ip, port, to, create_func):
        lbin_base_t.__init__(self, rsvc, ip, port, to)
        self.ssvc = ssvc
        self.create_func = create_func

    def on_accept(self, handler_para, common_handler):
        return self.create_func(self.rsvc, self.ssvc, handler_para, common_handler)


class my_main_t(lmain_base_t):
    def __init__(self):
        self.bsvc = None #my_bin_t
        self.ssvc = None #my_sche_t
        self.mhttp = None #my_http_t
        self.csvc = None #my_bin_t
        self.sche_v3 = None #lUpScheV3
        self.access_log = None
        self.rpt = None #ReportLupscheInfo
        self.public_ipaddr = None


        abs_path = lmain_base_t.get_abs_path()
        base_name = lmain_base_t.get_base_name()
        self.conf_file_name = os.path.join(abs_path, '%s.xml' % (base_name))
        self.select_server = SelectServer()
        if not LupScheXML.create(self.conf_file_name, self.conf_callback):
            sys.exit(0)
        strv =  lup_sche_xml.g_conf_info.listen['host'] 
        public_ip = lutil.get_public_ip(strv)
        if '' == public_ip:
            public_ip = socket.gethostname()
        self.public_ipaddr = public_ip
        print('public_ip:%s' % str(public_ip))
        public_addr = (public_ip, int(lup_sche_xml.g_conf_info.listen['bin_port']))
        log_server_addr = (lup_sche_xml.g_conf_info.log['log_server_ip'], int(lup_sche_xml.g_conf_info.log['log_server_port']))
        lmain_base_t.__init__(self, lup_sche_xml.g_conf_info.log['log_path'], log_server_addr, public_addr)
    
    def conf_callback(self, conf_info):
        self.select_server.set_conf_info(conf_info)
        LupScheXML.update(conf_info)
        if None != self.sche_v3:
            self.sche_v3.set_bitrate_video(conf_info.bitrate_video)
        conf_info.dump()

    def fini_base(self):
        pass


    def test_rpt(self):
        rpt_test = ReportLupscheInfo('10.10.69.195','8000','/tmp/rpt.pip') #Through the TCP transport
        rpt_test.send_v1('555##%^$$#$$@&#2356552aef33_test_msg','2333256677','4432','','10.10.69.120','10.10.69.120','')
        rpt_test.close()

    def init(self):

        logging.getLogger().setLevel(int(lup_sche_xml.g_conf_info.log['log_level']))
        lmain_base_t.set_log_mode(lup_sche_xml.g_conf_info.log['log_mode'])

        lup_sche_xml.g_conf_info.dump()

        logging.info("log ID is %s", str(self.public_addr))
        
        #self.test_rpt()
        self.rpt = ReportLupscheInfo(self.public_ipaddr, \
                lup_sche_xml.g_conf_info.listen['http_port'],\
                lup_sche_xml.g_conf_info.rpt['local_fifo_pipe']) #Through the TCP transport
       
        self.ssvc = my_sche_t(self.select_server)
        self.bsvc = my_bin_t(self.rsvc, self.ssvc,
                             '0.0.0.0', #lup_sche_xml.g_conf_info.listen['host'],
                             int(lup_sche_xml.g_conf_info.listen['bin_port']),
                             3 * 1000,
                             my_handler_t.create_handler)

        if 0 != int(lup_sche_xml.g_conf_info.listen['http_port']):
            self.mhttp = my_http_t(self.rsvc, self,
                                   '0.0.0.0', #lup_sche_xml.g_conf_info.listen['host'],
                                   int(lup_sche_xml.g_conf_info.listen['http_port']), self.rpt)

        #Support v3.1
        self.sche_v3 = self.__create_sche_v3()
        self.bsvc.attach(self.sche_v3)#my_bin_t,supported bin proto.
        self.mhttp.attach(self.sche_v3)#my_http_t,supported http proto.

        self.sche_v3.set_bitrate_video(lup_sche_xml.g_conf_info.bitrate_video)
        self.access_log = AccessLog(self.log_path,'lup_sche')
        lhttp_base_handler_t.access_log = self.access_log
        my_handler_t.access_log = self.access_log

    def __create_sche_v3(self):
        up_sche_res = UpScheRes()
        up_sche_res.on_up_sche = self.mhttp.on_up_sche_ex
        up_sche_res.on_up_rtmp_sche = self.mhttp.on_up_rtmp_sche_ex
        up_sche_res.on_repose_rtmp_client = self.mhttp.make_rtmp_upload_url_with_send
        up_sche_res.get_receiver_addr = self.ssvc.get_one_addr
        up_sche_res.get_receiver_ex_addr = self.ssvc.get_receiver_ex_addr
        up_sche_res.get_ants_addr = self.ssvc.get_notify_addr
        up_sche_res.rpt_to_statlog = self.rpt
        up_sche_res.upload_secret = lup_sche_xml.g_conf_info.upload_secret['private_key']
        up_sche_res.upload_backdoor_token = lup_sche_xml.g_conf_info.upload_secret['backdoor_token'] 
        up_sche_res.up_sche_secret =  lup_sche_xml.g_conf_info.up_sche_secret['private_key'] 
        up_sche_res.up_sche_token_timeout = int(lup_sche_xml.g_conf_info.up_sche_secret['token_timeout']) 
        return lUpScheV3(up_sche_res)

    def fini(self):
        pass

    def on_safe_quit(self):
        if None != self.bsvc:
            self.bsvc.stop()

        if None != self.mhttp:
            self.mhttp.stop()

    def can_safe_quit(self):
        if None != self.mhttp and None != self.bsvc:
            if self.mhttp.get_handler_count() <= 0:
                return self.bsvc.get_handler_count() <= 0
            else:
                return False
        return True

    def req_receiver_addr(self, seq_id, stream_id, user_id, token):
        pass

    def on_receiver_ack(self, seq_id, stream_id, result):
        pass

if __name__ == "__main__":
    m = my_main_t()
    lmain_base.main(m)
