import logging
import errno
import struct
import hashlib
import time
import traceback
import sys
import socket
import fcntl
import urlparse, copy, urllib

def create_listen_sock(ip, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setblocking(0)
    sock.bind((ip, port))
    sock.listen(256)

    return sock

def create_client_sock(server_ip, server_port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
    sock.setblocking(0)
    try:
        sock.connect((server_ip, int(server_port)))
    except ValueError:
        return None
    except socket.error, e:
        if e.errno == errno.EINPROGRESS:
            return sock
        return None

    return sock

def is_valid_ip_str(ip_str):
    ip_str = ip_str.strip()
    if len(ip_str) <= 0:
        return False
    try:
        socket.inet_pton(socket.AF_INET, ip_str)
        return True
    except socket.error:
        pass
    return False


def get_host_ip(hostname):
    hostname = hostname.strip()
    if len(hostname) <= 0:
        return ''

    if is_valid_ip_str(hostname):
        return hostname

    try:
        ip_str = socket.gethostbyname(hostname)
        return ip_str
    except socket.herror:
        logging.exception("can't resolve host %s", str(hostname))
    except socket.gaierror:
        logging.exception("can't resolve host %s", str(hostname))

    return ''

def ipstr2ip(ipstr):
    try:
        my = socket.inet_pton(socket.AF_INET, ipstr)
        (ret,) = struct.unpack('!I', my)
        return ret
    except socket.error:
        pass
    return -1

def ip2str(ip):
    try:
        packed_val = struct.pack('!I', int(ip))
        return socket.inet_ntoa(packed_val)
    except socket.error:
        pass
    return ''

# return (errno, buf)
def safe_recv(sock, sz, print_raw_err = False):
    if sz <= 0:
        return (0, '')

    tmp = ''
    ret_code = 0
    try:
        tmp = sock.recv(sz)
        if len(tmp) <= 0:
            ret_code = errno.EPIPE
    except KeyboardInterrupt, e:
        pass
    except socket.error, e:
        if e.errno not in [errno.EINTR, errno.EAGAIN, errno.EWOULDBLOCK]:
            ret_code = e.errno
            if print_raw_err:
                logging.debug("safe_recv errno:%s",  str(e))
    except:
        ret_code = errno.EIO
        if print_raw_err:
             logging.debug("safe_recv Exception errno:%s", str(ret_code))

    return (ret_code, tmp)

# return (errno, done_size)
def safe_send(sock, buf):
    if len(buf) <= 0:
        return (0, 0)

    ret_code = 0
    done_size = 0
    try:
        done_size = sock.send(buf)
        if 0 == done_size:
            ret_code = errno.EPIPE
    except KeyboardInterrupt, e:
        pass
    except socket.error, e:
        if e.errno not in [errno.EINTR, errno.EAGAIN, errno.EWOULDBLOCK]:
            ret_code = e.errno
    except:
        ret_code = errno.EIO
    return (ret_code, done_size)


def get_public_ip(eth_name):
    SIOCGIFADDR = 0x8915
    sock = None
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        ifreq = struct.pack('@16sH46s', eth_name, socket.AF_INET, '\x00' * 46)
        ifreq = fcntl.ioctl(sock.fileno(), SIOCGIFADDR, ifreq)
        pat = '@16sHH4s'
        ip = struct.unpack(pat, ifreq[0:struct.calcsize(pat)])[3]
        return socket.inet_ntoa(ip)
    except:
        pass
    finally:
        if None != sock:
            sock.close()

    return ''


def safe_list_remove(l, v):
    try:
        l.remove(v)
    except ValueError:
        pass

def get_pub(eth_name):
    SIOCGIFAD = 0x8915
    sock = None
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        ifreq = struct.pack('@16sH46s', eth_name, socket.AF_INET, '\x00' * 46)
        ifreq = fcntl.ioctl(sock.fileno(), SIOCGIFADDR, ifreq)
        pat = '@16sHH4s'
        ip = struct.unpack(pat, ifreq[0:struct.calcsize(pat)])[3]
        return socket.inet_ntoa(ip)
    except:
        pass
    finally:
        if None != sock:
            sock.close()

    return ''


def dump_exception(log):
    print >> sys.stderr, time.asctime(), log, traceback.format_exc()

#url = "http://www.laifeng.com/room/129?id=123&abc=456&name=ooo&token=33434r"
#del_keys_set=('token','app_id')
#call: url_values_del(url, del_keys_set)
def url_values_del(url, del_keys_set):
    ret = []
    del_keys = del_keys_set
    u = urlparse.urlparse(url)
    qs = u.query
    pure_url = url.replace('?'+qs, '')
    qs_dict = dict(urlparse.parse_qsl(qs))
    
    tmp_dict = {}
    for k in qs_dict.keys():
        in_del_list = False
        for val in del_keys:
            if str(k) == str(val): 
               in_del_list = True
               break;
        if not in_del_list:
           tmp_dict[k] = qs_dict[k]
    tmp_qs = urllib.unquote(urllib.urlencode(tmp_dict))
    return pure_url + "?" + tmp_qs


