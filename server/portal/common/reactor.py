#coding:utf-8 
import select
import time
import errno
import logging
from data_struct_defs import *
from lutil_ex import *

IO_IN = select.EPOLLIN
IO_OUT = select.EPOLLOUT
IO_ERR = select.EPOLLERR

class EpollReactor(IReactor):
    def __init__(self):
        self.efd = select.epoll()
        self.last_tick = 0
        self.timer_map = {}
        self.fd_map = {}

    def add_fd(self, fd, io_handler, event=0):
        self.efd.register(fd, event)
        self.fd_map[fd.fileno()] = io_handler

    # self, fd, io_handler
    def remove_fd(self, fd, _):
        del self.fd_map[fd.fileno()]
        self.efd.unregister(fd)

    # self, fd, io_handler, event
    def set_fd_event(self, fd, _, event):
        self.efd.modify(fd, event)

    def register_timer(self, timer_handler):
        self.timer_map[timer_handler] = 1

    def remove_timer(self, timer_handler):
        del self.timer_map[timer_handler]

    def svc(self, timeout):
        if 0 == self.last_tick:
            self.last_tick = get_current_ms()

        try:
            active_list = self.efd.poll(timeout)
        except IOError, e:
            if errno.EINTR == e.errno:
                return

        #print "active", len(active_list)
        for (fd, event) in active_list:
            handler = self.fd_map.get(fd, None)
            if None != handler:
                handler.handle_io(event)
            else:
                logging.warning("can't find handler for fd %d",
                                int(fd))

        current_ms = get_current_ms()
        if current_ms - self.last_tick >= 10:
            self.last_tick = current_ms
            for t in self.timer_map.keys():
                t.handle_timer()
                t.handle_timeout()

def create_reactor():
    return EpollReactor()
