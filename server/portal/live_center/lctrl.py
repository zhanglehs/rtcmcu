#!/usr/bin/python
#coding:utf-8 
import os
import sys
import logging
import logging.handlers
import signal

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),os.path.pardir)))
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),os.path.pardir) + '/common'))
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),os.path.curdir) + '/common'))

import reactor
import lctrl_conf
import lutil
import socket
import atexit
import time

import lversion
from daemon import Daemon
from log_handler import LogHandler 
from stream_manager import *
from http_server import *
from self_ctrl import *
from bin_msg_handler import BinMsgHandler
from bin_server import BinServer

class Portal(Daemon, SelfCtrl):
    def __init__(self):
        base_name = os.path.basename(sys.argv[0]).split('.')[0]
        abs_path = os.path.dirname(os.path.abspath(__file__))
        SelfCtrl.__init__(self, abs_path, base_name, self.reload_conf, "lctrl.test.conf")

        out_file = self.gen_out_file(abs_path, 'stdout.log')
        err_file = self.gen_out_file(abs_path, 'stderr.log')
        pidfile = self.gen_pid_file()
        Daemon.__init__(self, pidfile, stdout='/dev/null', stderr='/dev/null')
        
        self.log_handler = None
        self.rsvc = None
        self.hsvc = None
        self.bsvc = None
        self.sm = None


    def reload_conf(self, file_name):
        try:
            lctrl_conf.load_conf(file_name)
            self.log_handler.set_log(lctrl_conf.log_level, lctrl_conf.log_mode)
        except Exception as e:
            logging.exception("Portal::reload_conf exception:%s", str(e))

    def init(self):
        signal.signal(signal.SIGPIPE, signal.SIG_IGN)
        self.load_conf()
        self.rsvc = reactor.create_reactor()
        self.create_local_socket()
     
        eth_ip = lutil.get_public_ip('eth0')
        lctrl_conf.dump_conf()

        kwargs = {'log_server_addr':[lctrl_conf.reporter_ip, lctrl_conf.reporter_port],
                  'log_level_name' : lctrl_conf.log_level,
                  'store_path': lctrl_conf.log_path,
                  'local_addr' : eth_ip + ':' + str(lctrl_conf.http_port),
                  'log_mode': lctrl_conf.log_mode,
                  'app_name': 'lctrl'
                  }
        self.log_handler = LogHandler(**kwargs)

        logging.info("Portal (%s) start at %s ...", lversion.lc_version, eth_ip)
   
        stream_args = ()
        stream_kwargs = {}
        self.sm = StreamManager(*stream_args, **stream_kwargs)
        self.sm.subscribe_stream_change(self.log_handler.get_report_obj())
        self.sm.startup(self.rsvc, lctrl_conf.db_server_ip, lctrl_conf.db_server_port, lctrl_conf.db_server_idx)

        try:
            self.hsvc = HttpServer(self.rsvc,
                                      lctrl_conf.http_ip,
                                      lctrl_conf.http_port,
                                      self.log_handler,
                                      self.sm)

            self.bsvc = BinServer(self.rsvc,
                                    lctrl_conf.bin_ip,
                                    lctrl_conf.bin_port,
                                    self.sm)

        except:
            self.sm.stop()
            raise

    def run(self):
        try:
            self.init()

            try:
                while True:
                    self.rsvc.svc(100)
                    if self.safe_quit and self.hsvc.get_handler_count() <= 0:
                        break
            except Exception as e:
                try:
                    lutil.dump_exception("Portal::run find exception" + str(e))
                    logging.exception("Portal::run find exception:" + str(e))
                except Exception as e:
                    lutil.dump_exception("Portal::run find nested exception" + str(e))

            self.fini()

        except:
            try:
                lutil.dump_exception("Portal::run find exception")
                logging.exception("Portal::run find exception")
            except:
                lutil.dump_exception("Portal::run find nested exception")

    def fini(self):
        self.sm.stop();
        logging.info("Portal::run exit.")


if __name__ == "__main__":
    logging.raiseExceptions = False
    daemon = Portal()
    if len(sys.argv) > 2:
        if 'start' == sys.argv[1]:
            daemon.start()

	elif 'direct_run' == sys.argv[1]:
            daemon.run()

        elif 'stop' == sys.argv[1]:
            daemon.stop()
        elif 'force_stop' == sys.argv[1]:
            daemon.stop()
        elif 'reload_conf' == sys.argv[1]:
            daemon.send_cmd('reload_conf')
        elif 'version' == sys.argv[1]:
            print 'version:', lversion.lc_version
            sys.exit(0)
        else:
            print "Unknown command"
            sys.exit(2)
        sys.exit(0)
    else:
        print "usage: %s start|stop|force_stop|version lctrl.test.conf" % sys.argv[0]
        daemon.run()
        sys.exit(2)
