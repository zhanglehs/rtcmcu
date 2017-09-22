import logging
import errno
import time
import traceback
import sys
import httplib, urllib



def notify_service_start(ip, port, src, src_type, dst_sid, dst_ip, dst_port, dst_format, b_v=None, b_a=None, size=None):
    params_map = {'src': src, 'src_type': src_type, 'dst_sid': dst_sid, 'dst_host':dst_ip, 'dst_port':dst_port, 'dst_format':dst_format}
    if b_v != None and b_v !='':
        params_map['b_v']=b_v
    if b_a != None and b_a != '':
        params_map['b_a']=b_a
    if size != None and size !='':
        params_map['size']=size
    params = urllib.urlencode(params_map)
    print params
   #headers = {"Content-type": "application/x-www-form-urlencoded",
    #       "Accept": "text/plain"}
    url = "%s:%s" % (ip, port)
    logging.info("notify url is %s", url)
    try:
        conn = httplib.HTTPConnection(ip, int(port), 5)
        conn.request("GET", "/start?"+params)
        response = conn.getresponse()
        logging.info("http get result : %s ----%s", response.status, response.reason)
        print ("http get result : %s ----%s")% (response.status, response.reason)
        return response.status
    except Exception, e:
        print e
        print traceback.format_exc()
        logging.info("http get error, params : %s", params)


def notify_service_stop(ip, port, dst_sid):
    params = urllib.urlencode({'dst_sid': dst_sid})
    #headers = {"Content-type": "application/x-www-form-urlencoded",
    #       "Accept": "text/plain"}
    url = "%s:%s" % (ip, port)
    logging.info("notify url is %s", url)
    try:
        conn = httplib.HTTPConnection(ip, int(port), 5)
        conn.request("GET", "/stop?"+params)
        response = conn.getresponse()
        logging.info("http get result : %s ----%s", response.status, response.reason)
        print ("http get result : %s ----%s")% (response.status, response.reason)
        return response.status
    except Exception, e:
        print e
        print traceback.format_exc()
        logging.info("http get error, params : %s", params)




#notify_service_start("10.10.69.120", 2009, "aaa", "bbb", "ccc", "ddd", "eee", "fff", "111K", "23k", "2")
