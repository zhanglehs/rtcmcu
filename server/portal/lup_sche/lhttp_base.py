import reactor
import logging
import lutil
import urlparse
import json
import lversion
import zlib
import socket

MAX_TIMEOUT = 1000 * 10
REQ_TIMEOUT = 1000 * 3

HTTP_MSG = {}
HTTP_MSG[200] = "OK"
HTTP_MSG[302] = "Moved Temporarily"
HTTP_MSG[400] = "Bad Request"
HTTP_MSG[404] = "Not Found"
HTTP_MSG[500] = "Internal Server Error"
HTTP_MSG[405] = "Method Not Allowed"
ECODE_FAIL = 1
ECODE_INVALID_PARA = 2

def format_http_header(code, content_len, extra_header = "", cont_type = "text/plain"):
    msg = HTTP_MSG.get(code, 'Unknown')
    return "HTTP/1.1 %d %s\r\n\
Server: %s\r\n\
Content-Length: %d\r\n\
Connection:close\r\n\
Content-Type:%s\r\n\
%s\
\r\n" % (code, msg, lversion.lc_server, content_len, cont_type, extra_header)

def generate_json(error_code, extra_dict = None):
    json_dict = {}
    if None != extra_dict:
        json_dict = dict(extra_dict)
    json_dict['version'] = '1.0'
    json_dict['error_code'] = error_code
    return json.JSONEncoder().encode(json_dict)

def parse_rsp_header(http_head):
    line_ls = http_head.split('\r')
    ret_code = 0
    first_line = line_ls[0]
    ls = first_line.split()
    try:
        ret_code = int(ls[1])
    except ValueError:
        return (0, -1)

    for line in line_ls:
        line = line.strip()
        sections = line.split(':')
        if len(sections) != 2:
            continue
        if 'content-length' == sections[0].strip().lower():
            try:
                return (ret_code, int(sections[1].strip()))
            except ValueError:
                pass

    return (ret_code, -1)

def parse_req_header(header_buf):
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


class lhttp_base_handler_t(reactor.io_handler_t):
    access_log = None
    def __init__(self, http, reactor_svc, sock, addr):
        self.http = http
        self.sock = sock
        self.addr = addr
        self.reactor = reactor_svc
        self.reactor.add_fd(self.sock, self, reactor.IO_IN)
        self.rbuf = ""
        self.first_line = ""
        self.wbuf = ""
        self.wbytes = 0
        self.start_tick = reactor.get_current_tick()
        self.timeout_tick = self.start_tick + MAX_TIMEOUT

	def get_client_addr(self):
		return self.addr

    def on_handler_die(self):
        self.reactor.remove_fd(self.sock, self)
        self.sock.close()
        self.sock = None
        self.http.on_handler_die(self)
        if 0 == len(self.wbuf) and self.wbytes > 0:
            #TODO log
            pass

    def send_rsp(self, wbuf, code = 200, cont_type = "text/plain", extra_header = ""):
        self.wbuf = format_http_header(code, len(wbuf), extra_header, cont_type)
        self.wbuf += wbuf
        self.reactor.set_fd_event(self.sock, self, reactor.IO_OUT)
        logging.info('self:%s sock:%s send_rsp:%s', str(self),str(self.sock),str(self.wbuf))

   

    def handle_req(self, header_buf):
        logging.info("handle_req: sock:%s", str(self.sock))
        (first_line, headers) = parse_req_header(header_buf)
        #first_line = self.rbuf[0:self.rbuf.find("\r\n")].strip()
        try:#add access_log
             self.access_log.add_v3(first_line, self.addr)
        except Exception as e:
             logging.info('access_log.add_v3 exeception,e:%s',str(e))

        result = first_line.split()
        if 3 != len(result):
            logging.warning("can't parse http request from %s for %s",
                            self.addr[0], first_line)
            self.wbuf = format_http_header(500, 0)
            self.reactor.set_fd_event(self.sock, self, reactor.IO_OUT)
            return 

        ret_code = 405
        self.first_line = first_line.strip()
        if result[0].upper().strip() == "POST":
            self.reactor.set_fd_event(self.sock, self, 0)

            o = urlparse.urlparse(result[1].strip())
            para_m = urlparse.parse_qs(o.query)
            ret_code = self.http.on_post(self, o.path, para_m, self.addr)
        elif result[0].upper().strip() == "GET":
            self.reactor.set_fd_event(self.sock, self, 0)

            o = urlparse.urlparse(result[1].strip())
            para_m = urlparse.parse_qs(o.query)
            ret_code = self.http.on_get(self, o.path, para_m, self.addr)
        logging.info('handle_req: ret_code:%s',str(ret_code))

        if 0 != ret_code:
            self.wbuf = format_http_header(ret_code, 0)
            self.reactor.set_fd_event(self.sock, self, reactor.IO_OUT)

    def handle_io(self, evt):
        self.timeout_tick = reactor.get_current_tick() + MAX_TIMEOUT
        if evt & reactor.IO_IN:
            (ecode, tmp) = lutil.safe_recv(self.sock, 4 * 1024)
            if 0 != ecode:
                logging.debug("find conn error.code:%d", ecode)
                self.on_handler_die()
                return
            self.rbuf += tmp
            idx = self.rbuf.find("\r\n\r\n")
            if -1 != idx:
                #self.handle_req()
                header_buf = self.rbuf[0:idx]
                self.handle_req(header_buf)
            elif len(self.rbuf) >= 4 * 1024:
                self.on_handler_die()
                return

        elif evt & reactor.IO_OUT:
            (ecode, num) = lutil.safe_send(self.sock, self.wbuf)
            logging.info('self:%s encode:%s num:%s sock:%s wbuf:%s ',str(self),str(ecode), str(num), str(self.sock),str(self.wbuf))
            if 0 != ecode:
                logging.debug("find conn error.code:%d", ecode)
                self.on_handler_die()
                return

            self.wbytes += num
            self.wbuf = self.wbuf[num:]
            if 0 == len(self.wbuf):
                self.reactor.set_fd_event(self.sock, self, reactor.IO_IN)
                self.sock.shutdown(socket.SHUT_WR)
                self.timeout_tick = reactor.get_current_tick() + 3 * 1000
        else:
            self.on_handler_die()


class lhttp_base_t(reactor.io_handler_t,
              reactor.timer_handler_t):
    def __init__(self, reactor_svc, ip, port):
        self.sock = lutil.create_listen_sock(ip, port)

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
        return len(self.handler_list)

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
                if None != fd:
                    fd.setblocking(0)
                h = lhttp_base_handler_t(self, self.reactor, fd, addr)
                self.handler_list.append(h)
            except socket.error:
                logging.exception("find exception.")

    def handle_timeout(self):
        tmp_list = self.handler_list
        curr_tick = reactor.get_current_tick()
        for h in tmp_list:
            if curr_tick >= h.timeout_tick:
                logging.info("find http from %s request %s.",
                             str(h.addr), h.first_line)
                h.on_handler_die()


    def on_post(self, handler, path, para_m, client_addr):
        return 400

    def on_get(self, handler, path, para_m, client_addr):
        return 400


class req_handler_t(reactor.io_handler_t):
    def __init__(self, rsvc, notify, stream_info, action):
        self.sock = None
        self.reactor = rsvc
        self.notify = notify
        self.active_tick = reactor.get_current_tick()
        self.wbuf = ''
        self.rbuf = ''
        self.rsp_code = 0
        self.rsp_head = ''
        self.content_len = 0

        self.url = ''
        self.data = ''
        self.stream_info = stream_info
        self.action = action

    def on_handler_die(self):
        if None != self.sock:
            self.reactor.remove_fd(self.sock, self)
            self.sock.close()
            self.sock = None

        if None != self.notify:
            self.notify.on_handler_die(self)
        self.notify = None

    def post_uri(self, url, data):
        self.active_tick = reactor.get_current_tick()

        o = urlparse.urlparse(url)
        ip_str = lutil.get_host_ip(o.hostname)
        if len(ip_str) <= 0:
            return False
        req_header = "POST %s HTTP/1.1\r\n\
Connection: close\r\n\
Host: %s\r\n\
User-Agent: %s\r\n\
Content-Length: %d\r\n\
\r\n\
%s"
        uri = "%s?%s" % (o.path, o.query)
        req_header %= (uri, o.netloc, lversion.lc_server, len(data), data)
        self.wbuf = req_header
        self.sock = lutil.create_client_sock(ip_str, o.port)
        self.reactor.add_fd(self.sock, self, reactor.IO_OUT)

        self.url = url
        self.data = data
        return True

    def handle_io(self, evt):
        self.active_tick = reactor.get_current_tick()
        if evt & reactor.IO_OUT:
            (ecode, num) = lutil.safe_send(self.sock, self.wbuf)
            if 0 != ecode:
                logging.debug("find conn error.code:%d", ecode)
                self.on_handler_die()
                return

            self.wbuf = self.wbuf[num:]
            if 0 == len(self.wbuf):
                self.reactor.set_fd_event(self.sock, self, reactor.IO_IN)
        elif evt & reactor.IO_IN:
            (ecode, buf) = lutil.safe_recv(self.sock, 4 * 1024)
            if 0 != ecode:
                logging.debug("find conn error.code:%d", ecode)
                self.on_handler_die()
                return

            self.rbuf += buf

            if len(self.rsp_head) <= 0:
                idx = self.rbuf.find('\r\n\r\n')
                if -1 != idx:
                    self.rsp_head = self.rbuf[0:idx]
                    (self.rsp_code, self.content_len) = parse_rsp_header(self.rsp_head)
                    #TODO support Transfer-Encoding: chunked
                    if -1 == self.content_len:
                        logging.info("unknown content_len")
                    self.rbuf = self.rbuf[idx + 4:]

            if len(self.rsp_head) > 0 and -1 != self.content_len:
                if len(self.rbuf) >= self.content_len:
                    self.reactor.set_fd_event(self.sock, self, 0)
                    self.notify.on_result(self, self.rsp_code, self.rbuf)
        else:
            self.on_handler_die()

def gen_req_json(stream_info, action):
    data = {}
    data['methods'] = 'switch_broadcast'
    data['source'] = 'live_portal'
    parms = {}
    parms['userid'] = stream_info.user_id_str
    parms['roomid'] = stream_info.room_id_str
    parms['cs_id']  = stream_info.stream_id
    parms['action'] = action
    data['parms'] = parms

    return json.JSONEncoder().encode(data)

def result_json(code):
    data = {}
    if code == 200:
        data['result'] = 'success'
    else:
        data['result'] = 'fail'
    data['code'] = code
    return json.JSONEncoder().encode(data)

