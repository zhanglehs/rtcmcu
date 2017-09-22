#coding:utf-8 
'''
@file ants_session.py
@brief Realization of ants communicationt.
@author renzelong
<pre><b>copyright: Youku</b></pre>
<pre><b>email: </b>renzelong@youku.com</pre>
<pre><b>company: </b>http://www.youku.com</pre>
<pre><b>All rights reserved.</b></pre>
@date 2014/08/14
@see  lup_sche_v3.py \n
'''
import httplib, urllib
import logging
import errno
import time
import traceback
import sys
import os
from stream_id_helper import *

class ConvertInfo:
    def __init__(self):
        self.ants_ip = None
        self.ants_port = None
        self.src = None
        self.src_type = None #v2 Compatible, added flv for v3.1
        self.dst_sid = None #v2 Compatible
        self.dst_sid_v2 = None #v3.1 supported
        self.dst_ip = None
        self.dst_port = None
        self.dst_format = None
        self.b_v = {}
        self.b_a = {}
        self.size = None #v2 Compatible
        self.resolution = None #v3.1 supported
        self.et = None
        self.dur = None
        self.nt = 0 #1:need transcoding


class AntsSession(object):
    """xm_ants protocl of class"""
    def __init__(self):
        self.info = ConvertInfo()
        self.resolution_list = []#list
        pass

    def __create_resolution(self, width, height):
         resol_tmp = []
         resol_tmp.append(width)
         resol_tmp.append(height)
         return resol_tmp


    def __get_height_by_resolution(self):
        reso_str = str(self.info.resolution)
        reso_str = reso_str.lower()
        pos = reso_str.index('x')
        if pos < 0:
            return False
        height = int(reso_str[pos + 1:])
        return height
        
    def __get_width_by_resolution(self):
        reso_str = str(self.info.resolution)
        reso_str = reso_str.lower()
        pos = reso_str.index('x')
        if pos < 0:
            return False
        width = int(reso_str[0:pos])
        return width
 
    def __calc_resolution(self, is_net_stream = False):
        if self.info.resolution != None:
            del self.resolution_list[:]
        if None == self.info.resolution or '' == str(self.info.resolution):
            return False

        reso_str = str(self.info.resolution)
        reso_str = reso_str.lower()
        pos = reso_str.index('x')
        if pos < 0:
            return False
        width = int(reso_str[0:pos])
        height = int(reso_str[pos + 1:])
        logging.info('__calc_resolution nt:%s is_net_stream:%s',str(self.info.nt), str(is_net_stream))
        if (True == is_net_stream) and (0 == int(self.info.nt)):
            del self.resolution_list[:]
            return True #net stream,and not transcoding
        logging.info('filter pass')
        if height >= 720:#1280X720
            if True == is_net_stream:
                self.resolution_list.append(self.__create_resolution(1280, 720))
            self.resolution_list.append(self.__create_resolution(960, 540))
            self.resolution_list.append(self.__create_resolution(672, 378))
            self.info.dst_format = 'flv'

        elif height >= 540  and height < 720:
            if True == is_net_stream:
                self.resolution_list.append(self.__create_resolution(960, 540))
            self.resolution_list.append(self.__create_resolution(672, 378))
            self.info.dst_format = 'flv'
        else:
            pass
        #elif width >= 672 and width < 960:
        #      self.resolution_list.append(self.__create_resolution(672, 378))
        #      self.info.dst_format = 'flv'
        #else:
        #    self.resolution_list.append(width, height)
        return True

    def __pack(self, video_type, is_orgion_stream = False):
         params = {}
         if is_orgion_stream:#for net stream
             params = {'src': self.info.src, 'src_type':  self.info.src_type, \
                     'dst_sid':  self.info.dst_sid_v2, 'dst_host': self.info.dst_ip,\
                     'acodec' : 'copy','vcodec': 'copy',\
                     'dst_port': self.info.dst_port, 'dst_format': self.info.dst_format }
             return params

         if None != self.info.dst_sid_v2: 
             params = {'src': self.info.src, 'src_type':  self.info.src_type, \
                     'dst_sid':  self.info.dst_sid_v2, 'dst_host': self.info.dst_ip,\
                     'dst_port': self.info.dst_port, 'dst_format': self.info.dst_format }
         else:
             params = {'src': self.info.src, 'src_type':  self.info.src_type, \
                     'dst_sid':  self.info.dst_sid, 'dst_host': self.info.dst_ip,\
                     'dst_port': self.info.dst_port, 'dst_format': self.info.dst_format }

         b_v = self.info.b_v.get(video_type, '0')
         if b_v != '0':
             params['b_v'] = b_v

       	 if  self.info.et != None and  self.info.et != '':
             params['et'] =  self.info.et
             
         if  self.info.dur != None and  self.info.dur != '':
             params['dur'] =  self.info.dur
         
         if  self.info.size != None and self.info.size != '':
            params['size'] =  self.info.size
         return params

    def __send_msg(self, request):
         try:
            url = self.get_ants_addr()
            logging.info("ants_addr: %s", url)
            rep = url.split(':')
            if len(rep) < 2:
               logging.info('__send_mgs failed! msg:%s url:%s' % ( str(request), url))
               return 404
            conn = httplib.HTTPConnection(rep[0], int(rep[1]), 5)
            conn.request("GET", request)
            response = conn.getresponse()
            logging.info("send_start http Request:%s .Respose result : %s ----%s", request, response.status, response.reason)
            return response.status
         except Exception as e:
            logging.info("http get error, Request : %s exception:%s", request, str(e))
            return 404

    @staticmethod
    def is_supported_resolution(resolution):
        try:
            reso_str = str(resolution)
            reso_str = reso_str.lower()
            pos = reso_str.index('x')
            if pos < 0:
                return False
            width = int(reso_str[0:pos])
            height = int(reso_str[pos + 1:])
            if height > 720 or height < 540:
                return False
            return True
        except Exception as e:
            logging.info('is_supported_resolution exception:%s',str(e))
        return False


    def is_need_ants_transcode(self):
         return len(self.resolution_list) >= 1

    def set_convert_info(self, convert_info):
         self.info = convert_info

    def get_ants_addr(self):
         url = "%s:%s" % (self.info.ants_ip, self.info.ants_port)
         return url
         
    def send_start_v1(self, is_net_stream = False):
        streamid_lst = []
        self.__calc_resolution(is_net_stream)
        res_len = len(self.resolution_list)
        if is_net_stream or res_len <= 0:
            self.startup_orgion_stream()
            streamid_lst.append(self.info.dst_sid_v2)
            if (is_net_stream and 0 == self.info.nt) or (res_len <= 0):
                return True, streamid_lst

        stream_helper = StreamIDHelper(self.info.dst_sid)

        for size in  self.resolution_list:
            self.info.dst_sid_v2 =  stream_helper.encode_by_height(int(size[1]))
            self.info.size = str(size[0]) + 'x' + str(size[1])
            defi_type = stream_helper.get_defi_type_by_height(int(size[1]))
            params = self.__pack(defi_type)
            streamid_lst.append(self.info.dst_sid_v2)
            encode_params = urllib.urlencode(params)
            self.__send_msg("/start?" + encode_params)
        return True, streamid_lst


    def send_start(self, is_net_stream = False, is_rtmp = False):
        streamid_lst = []
        self.__calc_resolution(is_net_stream)
        stream_helper = StreamIDHelper(self.info.dst_sid)
        logging.info('send_start:len:%s', str(len(self.resolution_list)))
        if len(self.resolution_list) <= 0:
            self.startup_orgion_stream()
            streamid_lst.append(self.info.dst_sid_v2)
 
        else:
            params = self.pack_all(stream_helper, is_net_stream, is_rtmp)
            if params == '':
                return True, streamid_lst
            encode_params = urllib.urlencode(params)
            self.__send_msg("/start?" + encode_params)
            tmp = params['dst_sid'].split(',')
            for i in range(len(tmp)):
                streamid_lst.append(tmp[i])
        return True, streamid_lst


    #打包，仅支持非网络流2路转码，网络流4路转码
    def pack_all(self, stream_helper, is_net_stream = False, is_rtmp = False):
        stream_count = len(self.resolution_list)
        size_lst = ''
        vcodec_lst = ''
        acodec_lst = ''
        b_v_lst = ''
        stream_lst = ''
        logging.info('is_rtmp:%s', str(is_rtmp))
        if is_net_stream:
            #acodec_lst = 'copy,libfaac,libfaac,libfaac'
            acodec_lst = 'copy,copy,copy,copy'
            vcodec_lst = 'copy,libx264,libx264,libx264'
            b_v_lst = '%s,%s,%s,%s' % (self.info.b_v.get(DefiType.ORGIN, '400k'),
                                self.info.b_v.get(DefiType.SMOOTH, '400k'),
                                self.info.b_v.get(DefiType.SD, '800k'),
                                self.info.b_v.get(DefiType.HD, '1200k'))
            stream_lst += stream_helper.encode(DefiType.ORGIN)
            stream_lst += ','
            stream_lst += stream_helper.encode_by_resolution(540)
            stream_lst += ','
            stream_lst += stream_helper.encode_by_resolution(960)
            stream_lst += ','
            stream_lst += stream_helper.encode_by_resolution(1280)

            size_lst = '672x378,672x378,960x540,1280x720'

        else:#非网络流不变
            height =  self.__get_height_by_resolution()
            if False == is_rtmp:
                if height >= 720:
                    b_v_lst = '%s,%s' % (self.info.b_v.get(DefiType.SMOOTH, '400k'),
                                    self.info.b_v.get(DefiType.SD, '800k'))
                    stream_lst += stream_helper.encode_by_resolution(540)
                    stream_lst += ','
                    stream_lst += stream_helper.encode_by_resolution(960)
                    size_lst = '672x378,960x540'
                elif height >= 540:
                    b_v_lst = '%s' % (self.info.b_v.get(DefiType.SMOOTH, '400k'))
                    stream_lst += stream_helper.encode_by_resolution(540)
                    size_lst = '672x378'
                else:
                    return ''
            else:#rtmp plugin upload 
                if height >= 720:
                    b_v_lst = '%s,%s,%s' % (self.info.b_v.get(DefiType.ORGIN, '1300k'),
                                    self.info.b_v.get(DefiType.SMOOTH, '400k'),
                                    self.info.b_v.get(DefiType.SD, '800k'))

                    stream_lst += stream_helper.encode(DefiType.ORGIN)
                    stream_lst += ','

                    stream_lst += stream_helper.encode_by_resolution(540)
                    stream_lst += ','

                    stream_lst += stream_helper.encode_by_resolution(960)
                    size_lst = '1280x720,672x378,960x540'
                    acodec_lst = 'copy,copy,copy'
                    vcodec_lst = 'copy,libx264,libx264'

                elif height >= 540:
                    b_v_lst = '%s,%s' % (self.info.b_v.get(DefiType.ORGIN, '1300k'),
                            self.info.b_v.get(DefiType.SMOOTH, '400k'))
                    stream_lst += stream_helper.encode(DefiType.ORGIN)
                    stream_lst += ','

                    stream_lst += stream_helper.encode_by_resolution(540)
                    size_lst = '1280x720,672x378'
                    acodec_lst = 'copy,copy'
                    vcodec_lst = 'copy,libx264'
                else:
                    return ''



        '''
        for size in  self.resolution_list:
            if size_lst != '':
                size_lst += ','
            size_lst += str(str(size[0]) + 'x'+ str(size[1]))
        '''
        params = {}
        params['src'] =  self.info.src
        params['src_type'] =  self.info.src_type
        params['dst_sid'] =  stream_lst
        params['dst_host'] =  self.info.dst_ip
        params['dst_port'] =  self.info.dst_port
        params['dst_format'] =  self.info.dst_format
        params['size'] =  size_lst

        #temp test
        if is_net_stream or is_rtmp:
           params['vcodec'] =  vcodec_lst
           params['acodec'] =  acodec_lst
        params['b_v'] =  b_v_lst
       
        return params
          

    def startup_orgion_stream(self):
        logging.info('startup_orgion_stream start')
        stream_helper = StreamIDHelper(self.info.dst_sid)
        self.info.dst_sid_v2 =  stream_helper.encode(DefiType.ORGIN)
        params = self.__pack(DefiType.ORGIN, True)
        encode_params = urllib.urlencode(params)
        self.__send_msg("/start?" + encode_params)
        return True



    def send_stop(self):
         encode_params = urllib.urlencode({'dst_sid': self.info.dst_sid, 'dst_sid_v2': self.info.dst_sid_v2})
         return __send_msg("/stop?" + encode_params)
         
