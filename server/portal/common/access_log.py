#coding:utf-8
import logging
import os
import sys
import time
import threading
import Queue 

class AccessLog:
    def __init__(self, store_path,app_name = ''):
        self.__path = store_path
        self.__file_name = ''
        self.__app_name = app_name
        self.__f = None
	self.__wait_write_queue = Queue.Queue(maxsize = 2000)
       
    def __check_split_file(self):
        new_file_name = self.__gen_file_name()
        if self.__file_name == None or self.__file_name != new_file_name:
            f = None
            try:
                f = open(new_file_name, 'a+')
            except Exception as e:
                logging.info("open file %s failed. error = %s", new_file_name, str(e))

            if self.__f:
                self.__f.close() 
            self.__f = f
            self.__file_name = new_file_name


    def __gen_file_name(self):
        now = time.localtime(time.time())
        now_str = time.strftime('%Y-%m-%d', now)
        file_name = '%s/%s_access_%s.log' % ( self.__path, self.__app_name, now_str) 
        return file_name

    def __write_file(self, content):
        self.__check_split_file()
        if self.__f != None:
           self.__f.write( str(content))
           self.__f.flush()

    def __now_time_str(self):
        now = time.localtime(time.time())
        now_str = time.strftime('%Y-%m-%d:%H:%M:%S %Z', now)
        return now_str

    def add(self,ctx):
        alog = ''
        try:
            rsp_content = ctx['rsp_content']
            if len(rsp_content) > 1024:
                rsp_content = rsp_content[0:1024] + '...'
            alog = "%s - - [%s] '%s' %d %d '%s' '%s'\n" % (ctx['remote_addr'], self.__now_time_str(), ctx['first_line'], ctx['rsp_code'],int(ctx['duration']), ctx['user_agent'], rsp_content)

        except KeyError:
            return

        try:
            self.__wirte_file(alog)
        except Exception as e:
            logging.exception('exception:%s',str(e))

    def add_v1(self,req_info, remote_addr):
        try:
            ctx = self.get_ctx(req_info, remote_addr)
            alog = "%s requester:%s %s \n" % ( self.__now_time_str(), str(remote_addr), str(ctx['first_line']) )
            self.__write_file(alog)
        except Exception as e:
            logging.info('AccessLog::add_v1 exception:%s',str(e))

    def add_v2(self,req_info, remote_addr, rsp_code, rsp_content):
        try:
            ctx = self.get_ctx(req_info, remote_addr)
            alog = "%s - - [%s] %s %s %s %s %s %s\n" % ( str(remote_addr), self.__now_time_str(), str(ctx['first_line']),\
                    str(ctx['user_agent']), str(ctx['headers']), str(rsp_code), str(rsp_content), self.__now_time_str())
            self.__wirte_file(alog)
        except Exception as e:
            logging.exception('AccessLog::add_v2 exception:%s',str(e))

    def add_v3(self, first_line, remote_addr):
        try:
            addr =  str(remote_addr[0]) + ':' + str(remote_addr[1])
            alog = "%s requester:%s %s \n" % ( self.__now_time_str(), addr, str(first_line) )
            self.__write_file(alog)
        except Exception as e:
            logging.info('AccessLog::add_v3 exception:%s',str(e))

    def asyc_add(self, first_line, remote_addr):
	try:
	    msg = self.__now_time_str() + " requester:" + str(remote_addr[0]) + ':' + str(remote_addr[1]) + ' ' + str(first_line) + '\n'
	    self.__wait_write_queue.put(msg, 0)
	except Exception as e:
	    print('AccessLog::asyc_add exception:%s',str(e))

    def __asyc_wirte(self):
	try:
	    msg = self.__wait_write_queue.get()
	    if msg == None:
	       return
	    self.__write_file(msg)
	except Exception as e:
	   print('AccessLog::__asyc_wirte exception:%s',str(e))

    def __asy_wirte_thread(self):
	while 1:
	   try:
	      self.__asyc_wirte()
	   except Exception as e:
	      pass
	   time.sleep(1)

    @staticmethod
    def startup_syc_write_thread(owner):
	acc_log = owner
	acc_log.__asy_wirte_thread()

    def startup(self):
	t = threading.Thread(target=AccessLog.startup_syc_write_thread,args=(self,))
	t.setDaemon(True)
	t.start()

    def __parse_req_header(self, header_buf):
        idx = header_buf.find("\r\n")
        if -1 == idx:
            return ('', {})
        first_line = header_buf[0:idx].strip()
        left = header_buf[idx:].strip()
        lines = left.split('\r')
        ret = {}
        for l in lines:
            l = l.strip()
            (k, _, v) = l.partition(':')
            k = k.strip().lower()
            if 0 == len(k):
                continue
            ret[k] = v.strip()
        return (first_line, ret)



    def get_ctx(self, header_buf, remote_addr):
        log_ctx = {}
        (first_line, headers) = self.__parse_req_header(header_buf)
        result = first_line.split()
        if 3 != len(result):
             logging.warning("can't parse http request from %s for %s", self.addr[0], first_line)
             self.log_ctx['rsp_code'] = 500
             return log_ctx
        log_ctx['first_line'] = first_line.strip()
        log_ctx['user_agent'] = headers.get('User-Agent', '')
        log_ctx['headers'] = headers
        log_ctx['remote_addr'] = remote_addr
        return log_ctx










