import reactor
import socket
import lutil
import struct
import logging
import lproto

MAX_TIMEOUT = 20 * 1000



class lbin_base_handler_t(reactor.io_handler_t):
    def __init__(self, reactor_svc, handler_para, common_handler):
        self.base = handler_para[0]
        self.sock = handler_para[1]
        self.addr = handler_para[2]
        self.rsvc = reactor_svc
        self.rsvc.add_fd(self.sock, self, reactor.IO_IN)

        self.rbuf = ""
        self.wbuf = ""
        self.active_tick = reactor.get_current_tick()
        self.__common_handler = common_handler

    def attach(self, common_handler):
        self.__common_handler = common_handler

    def detach(self):
        self.__common_handler = None

    def get_client_addr(self):
        return self.addr
    
    def close_handler_impl(self):
        if None != self.sock:
            self.rsvc.remove_fd(self.sock, self)
            self.sock.close()
            self.sock = None

    def __on_handler_die(self):
        self.close_handler_impl()
        self.on_handler_die()
        self.base.on_handler_die(self)

    def send_pkg(self, pkg):
        if None == self.sock or len(pkg) <= 0:
            return
        if 0 == len(self.wbuf):
            self.rsvc.set_fd_event(self.sock, self,
                                      reactor.IO_IN | reactor.IO_OUT)
        self.wbuf += pkg

    def __handle_pkg(self, pkg):
        cmd = 0
        try:
            #magic, ver, cmd, size
            head_size = struct.calcsize(lproto.comm_head_pat)
            if len(pkg) < head_size:
                return
            tmp_pkg = pkg[0:head_size]
            (_, _, cmd, _) = struct.unpack(lproto.comm_head_pat, tmp_pkg)
        except struct.error:
            logging.exception("find exception")
            return

        payload = pkg[head_size:]
        if lproto.s2s_dummy_cmd == cmd:
            pkg = lproto.gen_pkg(cmd, payload)
            self.send_pkg(pkg)
            return

        self.on_pkg_arrive(cmd, payload)

    def handle_io(self, evt):
        self.active_tick = reactor.get_current_tick()
        if evt & reactor.IO_IN:
            (ecode, tmp) = lutil.safe_recv(self.sock, 4 * 1024)
            if 0 != ecode:
                logging.debug("find conn error.code:%d", ecode)
                self.__on_handler_die()
                return
            self.rbuf += tmp
            while None != self.sock:
                pkg_len = lproto.parse_pkg_len(self.rbuf)
                if -1 == pkg_len:
                    return

                if (pkg_len < struct.calcsize(lproto.comm_head_pat)
                   or pkg_len >= lproto.MAX_PKG_SIZE):
                    logging.warning("find invalid pkg %d", pkg_len)
                    self.__on_handler_die()
                    return
                elif len(self.rbuf) >= pkg_len:
                    pkg = self.rbuf[0:pkg_len]
                    self.rbuf = self.rbuf[pkg_len:]
                    self.__handle_pkg(pkg)
                else:
                    break
        elif evt & reactor.IO_OUT:
            (ecode, num) = lutil.safe_send(self.sock, self.wbuf)
            if 0 != ecode:
                logging.debug("find conn error.code:%d", ecode)
                self.__on_handler_die()
                return
            self.wbuf = self.wbuf[num:]
            if 0 == len(self.wbuf):
                self.rsvc.set_fd_event(self.sock, self, reactor.IO_IN)
        else:
            logging.info("unknown evt %s.", str(evt))
            self.__on_handler_die()

    def on_pkg_arrive(self, cmd, pkg):
        pass

    def on_handler_die(self):
        #assert(None == self.sock)
        pass


class lbin_base_t(reactor.io_handler_t,
                  reactor.timer_handler_t):
    def __init__(self, reactor_svc, ip, port,
                 timeout = MAX_TIMEOUT):
        self.sock = lutil.create_listen_sock(ip, port)
        self.rsvc = reactor_svc
        self.rsvc.add_fd(self.sock, self, reactor.IO_IN)
        self.rsvc.register_timer(self)

        self.handler_list = []
        self.handler_timeout = timeout
        self.__common_handler = None

    def attach(self, common_handler):
        self.__common_handler = common_handler
        tmp_list = self.handler_list
        for h in tmp_list:
            h.attach(self.__common_handler)

    def detach(self):
        self.__common_handler = None
        tmp_list = self.handler_list
        for h in tmp_list:
            h.detach()

    def stop(self):
        if None != self.sock:
            self.rsvc.remove_fd(self.sock, self)
            self.sock.close()
            self.sock = None

    def get_handler_count(self):
        return len(self.handler_list)

    def handle_io(self, evt):
        if evt & reactor.IO_IN:
            sock = None
            addr = ('', 0)
            try:
                (sock, addr) = self.sock.accept()
                sock.setblocking(0)

            except socket.error:
                logging.exception("find exception.")
                return

            h = self.on_accept((self, sock, addr), self.__common_handler)
            if None != h:
                self.handler_list.append(h)
            else:
                sock.close()

    def on_handler_die(self, h):
        lutil.safe_list_remove(self.handler_list, h)

    def handle_timeout(self):
        curr_tick = reactor.get_current_tick()

        tmp_list = self.handler_list
        for h in tmp_list:
            if curr_tick - h.active_tick >= self.handler_timeout:
                logging.info("find handle %s timeout", str(h.addr))
                h.close_handler_impl()
                lutil.safe_list_remove(self.handler_list, h)

    # return handle
    # para: self, sock, addr
    def on_accept(self, dummy_handler_para, common_handler):
        return None
