#coding:utf-8
import socket
import reactor
import lutil
import lctrl_conf

from data_const_defs import *
from data_struct_defs import *
from http_msg_handler import *
from stream_manager import *
from lutil_ex import *

class HttpServer(IOHandler,TimerNotify):

    def __init__(self, reactor_svc, ip, port, access_log_handler, stream_manager):
        self.sock = lutil.create_listen_sock(ip, port)

        self.access_log_handler = access_log_handler
        self.stream_manager = stream_manager

        self.reactor = reactor_svc
        self.reactor.add_fd(self.sock, self, reactor.IO_IN)

        self.reactor.register_timer(self)

        self.handler_list = []

    def stop(self):
        if None != self.sock:
            self.reactor.remove_fd(self.sock, self)
            self.sock.close()
            self.sock = None

    def get_handler_count(self):
        return len(self.handler_list) + len(self.notify.handler_list)

    def handle_timeout(self):
        tmp_list = self.handler_list
        curr_tick = get_current_tick()
        for h in tmp_list:
            h.handle_timer()
            if curr_tick >= h.timeout_tick:
                logging.info("timeout find http from %s request %s.",
                             str(h.addr), h.first_line)
                h.on_handler_die()

    def on_handler_die(self, h):
        for i in range(len(self.handler_list)):
            if h == self.handler_list[i]:
                del self.handler_list[i]
                break

    def handle_io(self, evt):
        if evt & reactor.IO_IN:
            if None == self.sock:
                return
            try:
                (fd, addr) = self.sock.accept()
                fd.setblocking(0)
                h = HttpMsgHandler(self, self.reactor, fd, addr, self.access_log_handler, self.stream_manager)
                self.handler_list.append(h)
            except socket.error:
                logging.exception("find exception.")



