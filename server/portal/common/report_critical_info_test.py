import os
import time
import threading
import logging

class ReportCriticalInfo:
    def __init__(self,out_path):
        self.__out_path = out_path
        self.__fd = None
        self.__fd_rd = None
        self.__callback_read = None
        self.__is_exit = False
        self.__msg_tag = 'msg_start_pos'
        logging.warning('ReportCriticalInfo:pipe:' + out_path + '\n')


    def create(self, is_read = False):
        print self.__out_path
        if not os.path.exists(self.__out_path):
            ret = os.mkfifo(self.__out_path, 0666)
            logging.warning('report_critical_info::create mkfifo ret:%s',str(ret))
	    if is_read:
	        #self.__fd  = os.open(self.__out_path,os.O_RDONLY|os.O_NONBLOCK) #1
	        self.__fd  = open(self.__out_path, 'r') #1
        else:
            #self.__fd_rd = os.open(self.__out_path,os.O_RDONLY|os.O_NONBLOCK)
            self.__fd  = os.open(self.__out_path,os.O_RDWR|os.O_NONBLOCK)
        logging.warning("report_critical_info::create fd:%s",str(self.__fd))
        if(self.__fd == None):
            return False
        return True

    def __pack(self, msg):
        ret_msg = ''
        ret_msg = '%s %s' % (str(self.__msg_tag), str(msg))
        return ret_msg

    def send(self, msg):
        logging.warning("report_critical_info::send msg:%s, __fd=%s", msg, str(self.__fd))
        if(self.__fd == None):
            return 0
        new_msg = self.__pack(msg)
        logging.warning('send msg: %s' % (new_msg))
        ret = os.write(self.__fd, new_msg)
        logging.warning('send end, ret:%s',str(ret))
        print("send ret:%s\n", str(ret));
        return ret
     
    def split(self, msg):
	    if msg != None and msg != '':
	       pos = msg.find(self.__msg_tag)
	    if -1 == pos:
	       return msg
	    pos += len(self.__msg_tag)
	    new_msg = str(msg[pos:]).strip()
	    return new_msg

    def __recv_worker(self):
        while not self.__is_exit:
           line = self.__fd.readline()#self.__read_line()
           if None != self.__callback_pfn and None != line:
               self.__callback_pfn(line)
           if line == '':
	      time.sleep(0.2)


    def recv(self, callback_pfn):
        self.__callback_pfn = callback_pfn
        t = threading.Thread(target=self.__recv_worker)
        t.start()

 
    def close(self):
        logging.warning("report_critical_info::close\n")
        if self.__fd != None:
            os.close(self.__fd);


def callback_recv_msg(msg):
	print msg


if __name__ == "__main__": 
    rpt = ReportCriticalInfo("/tmp/rpt.pip")
    rpt.create(False)
    #rpt.recv(callback_recv_msg)
    while True:
        rpt.send('10.10.69.195:80\tRPT\tlup_sche\t2014-11-24 10:57:15\t24913\t5341\t10.10.69.195\t\t\t\t10.10.69.121\n')
        time.sleep(0.5)
    rpt.close()


