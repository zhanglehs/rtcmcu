import os
import sys
import reactor
import logging
import logging.handlers
import signal
import lutil
import socket
import atexit
import traceback
from daemon import Daemon

g_log_mode = 'FILE'
class MyUDPLogger(logging.handlers.DatagramHandler):
    def emit(self, record):
        try:
            global g_log_mode
            if g_log_mode == 'FILE':
                return
            msg = self.format(record)
            #print msg
            #sys.stdout.flush()
            if len(msg) > 1400:
                msg = msg[0:1400]
            self.send(msg + "\n")
        except:
            print str(record)
            self.handleError(record)


class lmain_base_t(Daemon, reactor.io_handler_t):
    def __init__(self, log_path, log_server, public_addr):
        self.base_name = lmain_base_t.get_base_name()
        self.abs_path = lmain_base_t.get_abs_path()
        self.log_path = log_path
        self.log_server = log_server
        self.public_addr = public_addr
        out_file = lmain_base_t.gen_out_file(log_path, 'stdout.log')
        err_file = lmain_base_t.gen_out_file(log_path, 'stderr.log')
        pidfile = lmain_base_t.gen_pid_file(self.abs_path, self.base_name)
        Daemon.__init__(self, pidfile, stdout=out_file, stderr=err_file)

        self.sock_file = lmain_base_t.gen_sock_file(self.abs_path, self.base_name)
        self.rsvc = None
        self.sock = None
        self.safe_quit = False

    def del_sock_file(self):
        try:
            os.remove(self.sock_file)
        except OSError:
            pass

    @staticmethod
    def set_log_mode(mode):
        global g_log_mode
        g_log_mode = mode

    @staticmethod
    def get_abs_path():
        return os.path.dirname(os.path.abspath(__file__))

    @staticmethod
    def get_base_name():
        return os.path.basename(sys.argv[0]).split('.')[0]

    @staticmethod
    def gen_pid_file(abs_path, base_name):
        return os.path.join(abs_path, base_name + '.pid')

    @staticmethod
    def gen_sock_file(abs_path, base_name):
        return os.path.join(abs_path, base_name + '.sock')
        #return os.path.join('/tmp', base_name + '.sock')

    @staticmethod
    def gen_log_path(log_path):
        if not os.path.exists(log_path):
            os.makedirs(log_path, 0755)
        return log_path

    @staticmethod
    def gen_out_file(abs_path, filename):
        log_path = lmain_base_t.gen_log_path(abs_path)
        return os.path.join(log_path, filename)

    def gen_log_file(self):
        log_path = lmain_base_t.gen_log_path(self.log_path)
        return os.path.join(log_path,
                            "%s.log" % (self.base_name))

    @staticmethod
    def open_log(log_file, log_server, public_addr):
        fmt_str = '%(asctime)s %(levelname)-8s(%(filename)s:%(lineno)d)%(message)s'
        logging.basicConfig(format=fmt_str, filename='/dev/null')

        root = logging.getLogger()
        root.setLevel(logging.DEBUG)
        new_h = logging.handlers.TimedRotatingFileHandler(log_file, 'midnight')
        fmt = logging.Formatter(fmt_str)
        new_h.setFormatter(fmt)
        root.addHandler(new_h)

        udp_h = MyUDPLogger(log_server[0], log_server[1])
        log_id = "%s:%d" % public_addr
        fmt_str = log_id + "\tLOG\tlup_sche\t%(asctime)s\t%(levelname)s\t%(filename)s:%(lineno)d\t%(message)s"
        fmt = logging.Formatter(fmt_str)
        udp_h.setFormatter(fmt)
        root.addHandler(udp_h)


    def init_base(self):
        log_file = self.gen_log_file()
        lmain_base_t.open_log(log_file, self.log_server, self.public_addr)

        signal.signal(signal.SIGPIPE, signal.SIG_IGN)


        self.rsvc = reactor.create_reactor()

        self.del_sock_file()
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM, 0)
        self.sock.bind(self.sock_file)
        atexit.register(self.del_sock_file)
        self.rsvc.add_fd(self.sock, self, reactor.IO_IN)

    def handle_io(self, evt):
        if evt & reactor.IO_IN:
            (ecode, pkg) = lutil.safe_recv(self.sock, 1024)
            if 0 == ecode:
                self.dispatch_cmd(pkg)
        else:
            logging.warning("unknown event %d", evt)

    def send_cmd(self, cmd):
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM, 0)
        sock.sendto(cmd, 0, self.sock_file)

    def dispatch_cmd(self, pkg):
        pkg = pkg.strip()
        if "safe_quit" == pkg:
            self.safe_quit = True
            logging.info("trigger safe_quit")
            self.on_safe_quit()
        else:
            logging.info("unknown cmd %s", str(pkg))

    def run(self):
        try:
            self.init_base()
            logging.info("start...")
            self.init()

            while True:
                self.rsvc.svc(100)
                if self.safe_quit and self.can_safe_quit():
                    break

            self.fini()
            logging.info("end.")
            self.fini_base()
        except:
            try:
                print >> sys.stderr, traceback.format_exc()
                logging.exception("find exception")
            except:
                print >> sys.stderr, traceback.format_exc()

    def fini_base(self):
        pass

    def init(self):
        pass

    def fini(self):
        pass

    def on_safe_quit(self):
        pass

    def can_safe_quit(self):
        pass


def main(daemon):
    if len(sys.argv) == 2:
        if 'start' == sys.argv[1]:
            daemon.start()
        elif 'stop' == sys.argv[1]:
            daemon.send_cmd('safe_quit')
        elif 'force_stop' == sys.argv[1]:
            daemon.stop()
        elif 'run' == sys.argv[1]:
            daemon.run()
        else:
            print "Unknown command"
            sys.exit(2)
        sys.exit(0)
    else:
        print "usage: %s start|stop|force_stop" % sys.argv[0]
        sys.exit(2)
