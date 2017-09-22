import gevent
from gevent import socket
import time
import sys

class client(object):
    def __init__(self, speedtuple, remote_addr, req):
        self.speedtuple = speedtuple
        self.tuple_len = len(speedtuple)
        self.addr = remote_addr
        self.req = req
        self.sock = None
        self.seq = 0

    def connect(self):
        self.sock = socket.create_connection(self.addr, timeout=15)

    def run(self):
        start = time.clock()
        end = None
        total_bytes = 0
        speed = 0
        tmp = 0
        self.connect()
        self.sock.send(self.req)
        while True:
            speed = self.speedtuple[self.seq % self.tuple_len]
            total_bytes = 0
            end = start = time.clock()
            while total_bytes < speed and 0.4999 - (end - start) > 0:
                self.sock.settimeout(0.4999 - (end - start))
                try:
                    tmp = self.sock.recv(1024)
                    if not tmp:
                        print("connect close orderly...")
                        self.sock.close()
                        return
                    total_bytes += len(tmp)
                    end = time.clock()
                except socket.timeout as e:
                    break
                except Exception as e:
                    print("exception:%s", e)
                    self.sock.close()
                    return
            end = time.clock()
            if 0.5 - (end - start) > 0:
                gevent.sleep(0.5 - (end - start))
            self.seq += 1


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("%s [client_cnt]" % (sys.argv[0]))
        sys.exit(0)

    speedtuple = (50 * 512, 50 * 512, 50* 512, 50 * 512, 50 * 512, 80 * 512, 50 * 512)
    jobs = [gevent.spawn_later(i % 8 * 0.5, client(speedtuple, ("127.0.0.1", 8080), "GET /v1/s?a=98765&b=1 HTTP/1.0\r\n\r\n").run) for i in range(0, int(sys.argv[1]))]
    print("before join")
    gevent.joinall(jobs)
