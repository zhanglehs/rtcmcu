'''
@file lup_sche_v3.py
@brief implementation of lup_sche_v3, supported v3.1 project.
@author renzelong
<pre><b>copyright: Youku</b></pre>
<pre><b>email: </b>renzelong@youku.com</pre>
<pre><b>company: </b>http://www.youku.com</pre>
<pre><b>All rights reserved.</b></pre>
@date 2014/08/14
@see  lup_sche_v3_test.py \n
'''
import lproto
import logging
import struct
import os
import lutil
import time
import sys
import json
import const
from stream_id_helper import *
from ants_session import ConvertInfo
from ants_session import AntsSession
from report_lup_sche_info import ReportLupscheInfo 
from report_lup_sche_info import ScheInfo 

#src media format
const.SRC_TYPE_FLV = 0
const.SRC_TYPE_RTMP = 1
const.SRC_TYPE_RTMP_COMMON = 2
const.SRC_TYPE_RTP = 3

#http error code
const.HTTP_ERR_OK = 200
const.HTTP_ERR_BAD_REQUEST = 400
const.HTTP_ERR_UNAUTHORIZED = 401
const.HTTP_ERR_FORBIDDEN = 403
const.HTTP_ERR_NOT_FOUND = 404
const.HTTP_ERR_INTERNAL_SERVER_ERROR = 500
const.HTTP_ERR_NOT_IMPLEMENTED = 501

const.DEFAULT_RTMP_APP_NAME = "trm"

const.DEFAULT_BITRATE_VIDEO_SMOOTH = '350k'
const.DEFAULT_BITRATE_VIDEO_SD = '750k'
const.DEFAULT_BITRATE_VIDEO_HD = '1200k'

'''Resource information is required for the lUpScheV3 class'''
class UpScheRes:
    def __init__(self):
        self.on_up_sche = None
        self.on_up_rtmp_sche = None
        self.on_repose_rtmp_client = None
        self.get_receiver_addr = None
        self.get_receiver_ex_addr = None
        self.upload_secret = None
        self.up_sche_secret = None
        self.up_sche_token_timeout = None
        self.upload_backdoor_token = None
        self.get_ants_addr = None
        self.rpt_to_statlog = None
        self.bitrate_video = {}
        self.bitrate_video[DefiType.SMOOTH] = const.DEFAULT_BITRATE_VIDEO_SMOOTH
        self.bitrate_video[DefiType.SD] = const.DEFAULT_BITRATE_VIDEO_SD
        self.bitrate_video[DefiType.HD] = const.DEFAULT_BITRATE_VIDEO_HD

    def set_bitrate_video(self,bv_dict):
	    self.bitrate_video.clear()
	    self.bitrate_video[DefiType.SMOOTH] = bv_dict.get('SMOOTH', const.DEFAULT_BITRATE_VIDEO_SMOOTH)
	    self.bitrate_video[DefiType.SD] = bv_dict.get('SD', const.DEFAULT_BITRATE_VIDEO_SD)
	    self.bitrate_video[DefiType.HD] = bv_dict.get('HD', const.DEFAULT_BITRATE_VIDEO_HD)

'''The lUpScheV3 class is an implementation of the V3 protocol, supports both binary and HTTP protocol.'''
class lUpScheV3:
    def __init__(self, up_sche_res):
        self.__try_old_path_failed = True
        self.__up_sche_res = up_sche_res
        self.__sche_info = ScheInfo()
        self.__rpt_to_statlog = up_sche_res.rpt_to_statlog
 
    def set_bitrate_video(self, bv_dict):
	    self.__up_sche_res.set_bitrate_video(bv_dict)

    def __try_old_path(self, handler, para_m, client_addr, user_id_str, room_id_str, sche_token, stream_id_str, rtmp_app_str, source_format, resolution, nt = 0):
          #Keep the old processing logic, not by ants
          self.__try_old_path_failed = False
          if ('' == resolution or None == resolution or not AntsSession.is_supported_resolution(resolution) ) and const.SRC_TYPE_FLV == source_format:
              result = self.__up_sche_res.on_up_sche(handler, para_m, client_addr, user_id_str, room_id_str, sche_token ,stream_id_str, self)
              if 0 != result:
                   self.sche_result(handler,'','','', stream_id_str,result)
              return 0

          supported_old_rtmpsche = False
          try:
              r_arr = str(resolution).lower().split('x')
              if int(r_arr[1]) < 540:
                   supported_old_rtmpsche = True
          except Exception as e:
              logging.info('__try_old_path:%s', str(e))

          if const.SRC_TYPE_RTMP == source_format or (const.SRC_TYPE_RTMP_COMMON == source_format and supported_old_rtmpsche):#Multiplying temporarily does not support HD
               return self.__up_sche_res.on_up_rtmp_sche(handler, para_m, client_addr, user_id_str, room_id_str, sche_token, stream_id_str, rtmp_app_str)

          self.__try_old_path_failed = True
          return 0

    def sche_result(self, handler, ip_str,port,user_id_str, up_token, stream_id, result):
          try:
              ret_url = self.__gen_json_respose_context(ip_str, port, stream_id, up_token, result)
              handler.send_rsp(ret_url)
          except Exception as e:
              logging.info("__sche_result err for user_id(%s) and stream(%d) using HTTP err:%s", user_id_str, stream_id, str(e))


    def __check_token(self, stream_id, user_id_str, sche_token):
            curr_time = int(time.time())
            result = lutil.check_token(curr_time, stream_id,
                    user_id_str, '',
                    self.__up_sche_res.up_sche_secret,
                    self.__up_sche_res.up_sche_token_timeout, sche_token)

            if not result and sche_token != self.__up_sche_res.upload_backdoor_token:
                logging.warning("can't check sche_token %s for user_id(%s) and stream(%d) using HTTP", sche_token, user_id_str, stream_id)
                return False
            return True

    def __check_stream_id(self, stream_id_str):
            stream_id = 0
            try:
                stream_id = int(stream_id_str)
                if stream_id > 0:
                    return True
            except:
                pass
            logging.debug("invalid stream_id %s using HTTP", stream_id_str)
            return  False


    def __gen_sdp_url(self, sdp_ip, sdp_port, stream_id, token):
        sdp_url_format = 'http://%s:%s/upload/sdp/%s.sdp?token=%s'
        stream_id_long = str(stream_id)

	stream_id_short = str(stream_id)
	if len(stream_id_short) >= 16:
	    stream_id_short = str(int(stream_id_short[-16:], 16))
	sdp_token = self.make_token(stream_id_short, '2', sdp_ip, int(sdp_port))
        if len(stream_id_long) < 16:
            sid_helper = StreamIDHelper(int(stream_id))
            stream_id_long = sid_helper.encode(DefiType.ORGIN)
        sdp_url = sdp_url_format % (sdp_ip, str(sdp_port),stream_id_long,sdp_token)
        logging.info("__gen_sdp_url return:%s", sdp_url)
        return sdp_url


    def __gen_json_respose_context(self, dst_ip, dst_port, stream_id, token, result, sdp_port = 7087, is_rtp = False):
           data = {}
           data['ver'] = 'v3'
           if const.HTTP_ERR_OK == result:
               result = 0
           data['result'] = str(result)
           sdp_url = self.__gen_sdp_url(dst_ip, sdp_port, stream_id, token)
           if 0 == result: 
              params = {}
              params['uploader_ip'] = str(dst_ip)
              params['uploader_port'] = int(dst_port)
              params['stream_id']  = str(stream_id)
              params['token'] = token

              if is_rtp == True:
                  params['sdp_upload_url'] = sdp_url

              data['params'] = params
           else:
               pass

           return json.JSONEncoder().encode(data)

    def __gen_json_respose_context_for_net_stream(self, result):
           data = {}
           if const.HTTP_ERR_OK == result:
               result = 0
           data['result'] = str(result)
           return json.JSONEncoder().encode(data)

    def __send_respose_2_client(self, handler, stream_id, user_id_str, client_addr, is_rtp = False):
            dst_receiver_ex_ip_str = None
            dst_receiver_ex_uploader_port = None 
            dst_receiver_ex_player_port = None
            dst_receiver_ex_sdp_port = None
            try:
                if is_rtp == True:
                    (dst_receiver_ex_ip_str, dst_receiver_ex_uploader_port, dst_receiver_ex_sdp_port) = self.__up_sche_res.get_receiver_addr(stream_id, None, is_rtp)
                else:
                    (dst_receiver_ex_ip_str, dst_receiver_ex_uploader_port, dst_receiver_ex_player_port, dst_receiver_ex_sdp_port) = self.__up_sche_res.get_receiver_ex_addr(stream_id, None, is_rtp)
            except Exception as e:
                logging.info('lup_sche_v3::__send_respose_2_client Exception e:%s', str(e))
            logging.info('lup_sche_v3::__send_respose_2_client test1')
            receiver_ex_addr = []
            if '' == dst_receiver_ex_ip_str:
                logging.warning("can't get receiver_ex ip for user_id(%s) and stream(%d) using HTTP", user_id_str, stream_id)
                return receiver_ex_addr, False
            receiver_ex_addr.append(dst_receiver_ex_ip_str)
            receiver_ex_addr.append(dst_receiver_ex_uploader_port)
            receiver_ex_addr.append(dst_receiver_ex_player_port)
            receiver_ex_addr.append(dst_receiver_ex_sdp_port)
            
            try:
	        svr_port = dst_receiver_ex_uploader_port
                up_token = self.make_token(stream_id, user_id_str, dst_receiver_ex_ip_str, svr_port)
	        
                logging.info("return %s:%d for user_ip(%s) user_port(%s) user_id(%s) and stream(%d) using http",
                        dst_receiver_ex_ip_str, dst_receiver_ex_uploader_port, str(client_addr[0]), str(client_addr[1]), user_id_str, stream_id)
                #ret_url = 'http://%s:%d/v3/r?uid=%s&a=%s&b=%x' % (dst_receiver_ex_ip_str, dst_receiver_ex_uploader_port, user_id_str, up_token, stream_id)
                #eh = 'Location: %s\r\n' % (ret_url)
                #handler.send_rsp("", 302, "text/plain", extra_header = eh)
                ret_url = self.__gen_json_respose_context(dst_receiver_ex_ip_str, dst_receiver_ex_uploader_port, stream_id, up_token, const.HTTP_ERR_OK, dst_receiver_ex_sdp_port,is_rtp)
                handler.send_rsp(ret_url)
            except Exception as e:
                logging.info("__send_respose_2_client err for user_id(%s) and stream(%d) using HTTP err:%s", user_id_str, stream_id, str(e))
            return receiver_ex_addr, True

    def __send_respose_2_client_for_net_stream(self, handler, code):
            try:
                ret_val = self.__gen_json_respose_context_for_net_stream(code)
                handler.send_rsp(ret_val)
            except Exception as e:
                logging.info("__send_respose_2_client_for_net_stream code:%s err:%s", str(code), str(e))
                return False
            return True


    def __send_message_2_ants(self, stream_id, user_id_str, dst_receiver_ex_ip_str, dst_receiver_ex_player_port ,resolution, rtmp_url_play = None, is_rtp = False):
            (dst_receiver_ip_str, dst_receiver_port, sdp_port) = self.__up_sche_res.get_receiver_addr(stream_id, None,is_rtp)
            if '' == dst_receiver_ip_str:
                logging.warning("can't get receiver ip for user_id(%s) and stream(%d) using HTTP", user_id_str, stream_id)
                return const.HTTP_ERR_INTERNAL_SERVER_ERROR
            
            try:
                convert_info = ConvertInfo()

                convert_info.src_type = 'flv'
                is_rtmp = (rtmp_url_play != None)
                if not is_rtmp:
                    download_url = 'http://%s:%d/v1/s?a=98765&b=%x' % (dst_receiver_ex_ip_str, \
                                                                        dst_receiver_ex_player_port, \
                                                                        int(stream_id))
                else:
                    download_url = rtmp_url_play

                convert_info.src = download_url
                convert_info.dst_format = 'flv'
                convert_info.resolution = resolution
                convert_info.dst_ip = dst_receiver_ip_str
                convert_info.dst_port = dst_receiver_port
                convert_info.dst_sid = stream_id
                (convert_info.ants_ip, convert_info.ants_port) = self.__up_sche_res.get_ants_addr(stream_id)
                convert_info.b_v = self.__up_sche_res.bitrate_video
                ants_session = AntsSession()
                ants_session.set_convert_info(convert_info)
                result, self.__sche_info.streamid_list = ants_session.send_start(False, is_rtmp)
                if True == result and len(self.__sche_info.streamid_list) > 0:
                    self.__sche_info.ants_addr = str(convert_info.ants_ip) + ":" + str(convert_info.ants_port)
                    self.__sche_info.receiver_addr = str(convert_info.dst_ip) + ":" + str(convert_info.dst_port)
                return 0
            except Exception as e:
                logging.info("__send_message_2_ants err for user_id(%s) and stream(%d) using HTTP err:%s", user_id_str, stream_id, str(e))
            return const.HTTP_ERR_INTERNAL_SERVER_ERROR

    def __send_message_2_ants_for_net_stream(self, stream_id, src , src_type, resolution, et, dur, nt):
            (dst_receiver_ip_str, dst_receiver_port, sdp_port) = self.__up_sche_res.get_receiver_addr(stream_id)
            if '' == dst_receiver_ip_str:
                logging.warning("__send_message_2_ants_for_net_stream can't get receiver ip for stream(%d) using HTTP", stream_id)
                return const.HTTP_ERR_INTERNAL_SERVER_ERROR
            
            try:
                convert_info = ConvertInfo()
                
                convert_info.src = src
                convert_info.dst_format = 'flv'
                src_type = src_type.lower()
                if 'flv' == src_type or 'rtmp' == src_type:
                   convert_info.src_type = 'flv'
                else:
                   convert_info.src_type = src_type
                convert_info.resolution = resolution
                convert_info.dst_ip = dst_receiver_ip_str
                convert_info.dst_port = dst_receiver_port
                convert_info.dst_sid = stream_id
                convert_info.et = et
                convert_info.dur = dur
                convert_info.nt = nt
                (convert_info.ants_ip, convert_info.ants_port) = self.__up_sche_res.get_ants_addr(stream_id)
                convert_info.b_v = self.__up_sche_res.bitrate_video
                ants_session = AntsSession()
                ants_session.set_convert_info(convert_info)
                result, self.__sche_info.streamid_list = ants_session.send_start(True)
                if True == result and len( self.__sche_info.streamid_list) > 0:
                    self.__sche_info.ants_addr = str(convert_info.ants_ip) + ":" + str(convert_info.ants_port)
                    self.__sche_info.receiver_addr = str(convert_info.dst_ip) + ":" + str(convert_info.dst_port)
                return 0
            except Exception as e:
                logging.info("__send_message_2_ants_for_net_stream err for stream(%d) using HTTP err:%s", stream_id, str(e)) 
            return const.HTTP_ERR_INTERNAL_SERVER_ERROR

    def make_token(self, stream_id, user_id_str, dst_svr_ip_str, dst_svr_port):
            up_addr = '%s:%d' % (dst_svr_ip_str, dst_svr_port)
            curr_time = int(time.time())
            up_token = lutil.make_token(curr_time, stream_id, user_id_str, up_addr, self.__up_sche_res.upload_secret)
	    logging.info("make_token up_token:%s stream_id:%s user_id_str:%s dst_svr_ip_str:%s dst_svr_port:%s curr_time:%s, upload_secret:%s",
				str(up_token), str(stream_id), str(user_id_str), str(dst_svr_ip_str), str(dst_svr_port),
				str(curr_time), str(self.__up_sche_res.upload_secret))

            return up_token

    def make_token_for_uploader_request(self, stream_id, user_id_str):
            curr_time = int(time.time())
            up_token = lutil.make_token(curr_time, stream_id, user_id_str, '', self.__up_sche_res.up_sche_secret)
            return up_token

    def on_sche_for_bin(self, pkg, client_addr):
        try:#v3 not supported bin proto.
            curr_time = int(time.time())
            stream_id = int(pkg["streamid"])
            user_id_str = str(pkg["userid"])
            sche_token = pkg["sche_token"].rstrip('\x00')

            width = '0'
            height = '0'
            if True == pkg.has_key('width'):
                width = str(pkg['width'])
            if True == pkg.has_key('height'):   
                height = str(pkg['height'])

            #check token
            if not self.__check_token(stream_id, user_id_str, sche_token):
                return (1, 0, 0, '')

            #check stream_id
            if not self.__check_stream_id(str(stream_id)):
                return (1, 0, 0, '')

            dst_ip_str = None 
            dst_port = None
            if 0 == int(width) or 0 == int(height):
                 (dst_ip_str, dst_port, sdp_port) = self.__up_sche_res.get_receiver_addr(stream_id)
            else:
                 (dst_ip_str, dst_up_port, dst_down_port, sdp_port) = self.__up_sche_res.get_receiver_ex_addr(stream_id)
                 #send notify to ants server
                 resolution = width + 'x' + height
                 self.__send_message_2_ants(stream_id, user_id_str, dst_ip_str, dst_down_port, resolution)
                 dst_port = dst_up_port
       
            logging.info("return %s:%s for user_ip(%s) user_port(%s) user_id(%s) and stream(%d)",
                         dst_ip_str, str(dst_port), str(client_addr[0]), str(client_addr[1]), user_id_str, stream_id)
            up_token = self.make_token(stream_id, user_id_str, dst_ip_str, dst_port)
            return (0, dst_ip_str, dst_port, up_token)

        except Exception as e:
            pass
        return (1, 0, 0, '') 

    def on_sche_for_http(self, handler, para_m, client_addr):
        try:
            logging.info('on_sche_for_http params:%s', str(para_m))
            user_id_str = para_m.get('uid', [''])[0].strip()
            room_id_str = para_m.get('rid', [''])[0].strip()
            sche_token = para_m.get('a', [''])[0].strip()
            stream_id_str = para_m.get('b', [''])[0].strip()
            rtmp_app_str = para_m.get('c', [''])[0].strip()
            resolution = para_m.get('r', ['672x378'])[0].strip()

            if '' == rtmp_app_str:
                rtmp_app_str = const.DEFAULT_RTMP_APP_NAME

            source_format = int(para_m.get('f', ['0'])[0].strip()) #(0£ºflv 1£ºrtmp£©
    
            if ('' == user_id_str or '' == room_id_str or
                    '' == sche_token or '' == stream_id_str):
                logging.info("find invalid para using HTTP")
                return const.HTTP_ERR_BAD_REQUEST

            stream_id = int(stream_id_str)
            logging.info("on_sche_for_http start params: user_id:%s room_id:%s token:%s stream_id:%s rtmp_app:%s resolution:%s", 
                    str(user_id_str), str(room_id_str), str(sche_token), str(stream_id_str), str(rtmp_app_str), str(resolution))
            #Keep the old processing logic, not by ants
            ret_val = self.__try_old_path(handler, para_m, client_addr, user_id_str, room_id_str, sche_token, stream_id_str, rtmp_app_str,source_format, resolution)
            if False == self.__try_old_path_failed:
                return 0

            #check stream_id
            if not self.__check_stream_id(stream_id_str):
                return const.HTTP_ERR_BAD_REQUEST
            #check token
            if not self.__check_token(stream_id, user_id_str, sche_token):
                return const.HTTP_ERR_FORBIDDEN
        
            #send respose to uploader(client)
            upload_rtmp_server_addr = None
            receiver_ex_addr = ('', 80, 443)
            rtmp_url_play = None
            rtmp_url_pub = None
            is_rtp = (source_format == const.SRC_TYPE_RTP)
            self.__sche_info.user_id = user_id_str
            self.__sche_info.chn_id = stream_id_str
             

            if source_format == const.SRC_TYPE_RTMP_COMMON:
                (ecode, upload_rtmp_server_addr, rtmp_url_pub, rtmp_url_play) = self.__up_sche_res.on_repose_rtmp_client(handler, stream_id_str, user_id_str, rtmp_app_str)
                self.__sche_info.rtmpserver_addr = str(upload_rtmp_server_addr[0]) + ":" + str(upload_rtmp_server_addr[1])
                result = (ecode == 0)
                if False == result:
                     return const.HTTP_ERR_INTERNAL_SERVER_ERROR

            else:
                receiver_ex_addr, result = self.__send_respose_2_client(handler, stream_id, user_id_str, client_addr, is_rtp)
                if False == result:
                    return const.HTTP_ERR_INTERNAL_SERVER_ERROR

                if True == is_rtp:#RTP not supported HD
                    self.__sche_info.receiver_addr = receiver_ex_addr
                    self.__rpt_to_statlog.send_v2(self.__sche_info)
                    return 0
                self.__sche_info.receiver_ex_addr = receiver_ex_addr
                logging.info('on_sche_for_http receiver_ex_addr:%s len:%s',str(receiver_ex_addr), str(len(receiver_ex_addr)))

            logging.info('rtmp_url_play:' + str(rtmp_url_play))
            logging.info('source_format:' + str(source_format))
            try:
               #send notify to ants server
               self.__send_message_2_ants(stream_id, user_id_str, receiver_ex_addr[0],receiver_ex_addr[2],resolution, rtmp_url_play, is_rtp)
               self.__rpt_to_statlog.send_v2(self.__sche_info)
            except Exception as e:
                logging.info("on_sche_for_http report log error! e:%s", str(e))
            return 0
        except Exception as e:
            logging.info("on_sche_for_http error! e:%s", str(e))
        return const.HTTP_ERR_INTERNAL_SERVER_ERROR 
	    
 
    '''
    supported network streamming
    see to: http://wiki.1verge.net/happy:interlive:net_stream_lupsche
    '''
    def on_sche_for_net_stream(self, handler, para_m, client_addr):
        result = self._sche_for_net_stream(handler, para_m, client_addr)

        #send respose to uploader(client)
        self.__send_respose_2_client_for_net_stream(handler, result)

        return 0
       

    def _sche_for_net_stream(self, handler, para_m, client_addr):
        try:
            logging.info('_sche_for_net_stream params:%s', str(para_m))
            room_id_str = para_m.get('rid', [''])[0].strip()
            user_id_str = para_m.get('u', [''])[0].strip()
            sche_token = para_m.get('a', [''])[0].strip()
            stream_id_str = para_m.get('b', [''])[0].strip()
            resolution = para_m.get('r', [''])[0].strip()
            src_str = str(para_m.get('s', [''])[0]).strip()
            src_type_str = para_m.get('st', [''])[0].strip()
            dur_str = para_m.get('d', [''])[0].strip()
            et_str = para_m.get('e', [''])[0].strip()
            nt = para_m.get('nt', ['0'])[0].strip()
            if '' == src_str or '' == src_type_str or '' == stream_id_str:
                logging.info('_sche_for_net_stream params error!')
                return const.HTTP_ERR_BAD_REQUEST
            
            stream_id = int(stream_id_str)

            #check stream_id
            if not self.__check_stream_id(stream_id_str):
                return const.HTTP_ERR_BAD_REQUEST
        
            #check token
            if not self.__check_token(stream_id, user_id_str, sche_token):
                return const.HTTP_ERR_FORBIDDEN
            
            #send notify to ants server
            self.__send_message_2_ants_for_net_stream(stream_id, src_str, src_type_str, resolution, et_str, dur_str, nt)
            try:
                self.__sche_info.receiver_ex_addr = src_str
                self.__rpt_to_statlog.send_v2(self.__sche_info)
            except Exception as e:
                logging.info("_sche_for_net_stream report error! e:%s", str(e))
            return 0
        except Exception as e:
            logging.info("_sche_for_net_stream error! e:%s", str(e))
        return const.HTTP_ERR_INTERNAL_SERVER_ERROR


