# -*- coding:utf-8 -*-
'''
@file sche_algorith.py
@brief Server scheduling algorithm, suitable for lup_sche, lctrl.
@author renzelong
<pre><b>copyright: Youku</b></pre>
<pre><b>email: </b>renzelong@youku.com</pre>
<pre><b>company: </b>http://www.youku.com</pre>
<pre><b>All rights reserved.</b></pre>
@date 2014/09/01 \n
'''
import logging

svr_call_cnt_last = -1
class ScheAlgorith(object):
    def __init__(self):
        pass
    '''return True,{ip,port,asn,region}
       server_list eg:receiver_list list:[{'ip': '10.10.69.120', 'region': '11', 'uploader_port': '80', 'asn': '1'}, 
                                          {'ip': '10.10.69.121', 'region': '11', 'uploader_port': '80', 'asn': '1'}]
       key
    '''
    def get_server_hash_by_cnt(self, server_list):
        ret_server = {}
        logging.info('ScheAlgorith::get_server_hash_by_cnt:%s',str(server_list))
        if len(server_list) <= 0 :
            return {}
	global svr_call_cnt_last
	svr_call_cnt_last += 1
        ret_server = server_list[svr_call_cnt_last % len(server_list)]
        return ret_server


    '''return True,{ip,port,asn,region}
       server_list eg:receiver_list list:[{'ip': '10.10.69.120', 'region': '11', 'uploader_port': '80', 'asn': '1'}, 
                                          {'ip': '10.10.69.121', 'region': '11', 'uploader_port': '80', 'asn': '1'}]
       key
    '''
    def get_server_hash(self, server_list, key):
        ret_server = {}
        logging.info('ScheAlgorith::get_server_hash:%s, key:%s',str(server_list), str(key))
        if len(server_list) <= 0 or None == key :
            return {}
        ret_server = server_list[int(key) % len(server_list)]
        return ret_server


    '''return True,{ip,port,asn,region}
       server_list eg:receiver_list list:[{'ip': '10.10.69.120', 'region': '11', 'uploader_port': '80', 'asn': '1'}, 
                                          {'ip': '10.10.69.121', 'region': '11', 'uploader_port': '80', 'asn': '1'}]
    '''
    def get_local_server(self, server_list, local_ip):
        ret_server = {}
        if len(server_list) <= 0 or None == local_ip or '' == str(local_ip).strip(''):
            return False,{}
        for server in range(0, len(server_list),1):
            if local_ip != server['ip']:
                continue
            ret_server = server
            break
        return True, ret_server

    '''return True,{ip,port,asn,region}
       server_list eg:receiver_list list:[{'ip': '10.10.69.120', 'region': '11', 'uploader_port': '80', 'asn': '1'}, 
                                          {'ip': '10.10.69.121', 'region': '11', 'uploader_port': '80', 'asn': '1'}]
    '''
    def get_server_by_asn(self, server_list, asn):
        ret_server = {}
        if len(server_list) <= 0 or None == asn or '' == str(asn).strip(''):
            return False,{}
        for server in range(0, len(server_list),1):
            if asn != server['asn']:
                continue
            ret_server = server
            break
        return True, ret_server

    '''return True,{ip,port,asn,region}
       server_list eg:receiver_list list:[{'ip': '10.10.69.120', 'region': '11', 'uploader_port': '80', 'asn': '1'}, 
                                          {'ip': '10.10.69.121', 'region': '11', 'uploader_port': '80', 'asn': '1'}]
    '''
    def get_server_by_asn_region(self, server_list, asn, region):
        ret_server = {}
        if len(server_list) <= 0 or None == asn or '' == str(asn).strip('') or None == region or '' == str(region).strip(''):
            return False,{}
        for server in range(0, len(server_list),1):
            if asn != server['asn'] or region != server['region']:
                continue
            ret_server = server
            break
        return True, ret_server
         

