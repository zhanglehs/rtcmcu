#coding:utf-8 
import reactor
import socket
import lutil
import struct
import logging
import time
from proto import *

from data_struct_defs import *
from stream_manager import * 
from streaminfo_pack import StreamInfoListPack, StreamInfoPack
from stream_change_notify import IStreamChangeNotify
from bin_msg_handler import BinMsgHandler
from lutil_ex import *

stream_info_cmd_v2_cache = None #cache stream white list msg v2

class BinServer(IOHandler, TimerNotify, IStreamChangeNotify):
    def __init__(self, reactor_svc, ip, port, sm):
        self.sock = lutil.create_listen_sock(ip, port)
        self.reactor = reactor_svc
        self.reactor.add_fd(self.sock, self, reactor.IO_IN)
        self.reactor.register_timer(self)

        self.handler_list = []

        self.sm = sm
        self.sm.subscribe_stream_change(self)

    def handle_io(self, evt):
        if evt & reactor.IO_IN:
            try:
                (sock, addr) = self.sock.accept()
                sock.setblocking(0)
                logging.info('BinServer::handle_io accept:%s', str(addr))
                h = BinMsgHandler(self.reactor, sock, addr, self.sm, self.on_handler_die)
                self.handler_list.append(h)
            except socket.error as e:
                logging.exception("BinServer::handle_io find exception." + str(e))

    def on_handler_die(self, h):
        for i in range(len(self.handler_list)):
            if h == self.handler_list[i]:
                del self.handler_list[i]
                break

    def handle_timeout(self):
        tmp_list = self.handler_list
        curr_tick = get_current_tick()
        for h in tmp_list:
            if curr_tick - h.active_tick >= MAX_TIMEOUT:
                #logging.info("BinServer::handle_timeout find handle %s timeout", str(h.addr))
                h.on_handler_die()

    def __send_to_all(self, msg):
        tmp_list = self.handler_list
        for h in tmp_list:
            h.send_pkg(msg)

    def on_stream_create(self, si, do_create, is_hd):
        if not do_create:
            return
        #logging.info('BinServer::on_stream_create:si:%s do_create:%s is_hd:%s', si.to_dict(), str(do_create), str(is_hd))
        self.__send_to_all(gen_stream_start_pkg_v2(si.stream_id, si.start_time))
      

    def on_stream_destroy(self, si, do_destroy):
        if not do_destroy:
            return
        #logging.info('BinServer::on_stream_destroy: stream_id:%s ', si.stream_id)
        self.__send_to_all(gen_stream_stop_pkg_v2(si.stream_id))


