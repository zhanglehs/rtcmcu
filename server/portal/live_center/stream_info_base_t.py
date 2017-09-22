#encoding=utf-8
import time
import logging
import reactor
import copy
import lctrl_conf
import lutil
import json
import ujson
import datetime
from timeval_t import timeval_t 

class stream_info_propery_t:
    def __init__(self):
        self.alias = ''
        self.update_time = 0

    def clear(self):
        self.alias = ''
        self.update_time = 0



class stream_info_base_t:
    def __init__(self):
        self.si_map        = {} #map<app_id, map<stream_id, stream_info_propery_t>>
        self.json_str = ''
        self.is_updated = False

    def add_safe(self, stream_id, app_id_str, alias):
        try:
            return self.__add(stream_id, app_id_str, alias)
        except Exception as e:
            logging.exception('add_safe:%s', str(stream_id))
        return False

    def __add(self, stream_id, app_id_str, alias):
        logging.info('stream_info_base_t::__add stream_id:%s appid:%s alias:%s', str(stream_id), str(app_id_str), str(alias))
        if stream_id == None or str(stream_id).strip() == '' or \
           app_id_str == None or str(app_id_str).strip() == '':
               return False
        app = {}
        sip = stream_info_propery_t()
        sip.alias = alias
        sip.update_time = int(time.time())
        if self.si_map.has_key(app_id_str):
            app = self.si_map.get(app_id_str)
        else:
            self.si_map[app_id_str] = app
        app[stream_id] = sip
        self.is_updated = True
        return True
         
    def del_safe_ex(self, app_id, stream_id):
        try:
            self.__delete_ex(app_id, stream_id)
        except Exception as e:
            logging.exception('stream_info_base_t::del_safe:%s', str(e))

    def __delete_ex(self, app_id, stream_id):
        logging.info('stream_info_base_t::__delete_ex stream_id:%s appid:%s', str(stream_id), str(app_id))
        if not self.si_map.has_key(app_id):
            return
        app = self.si_map.get(app_id)
        if not app.has_key(stream_id):
            return
        del app[stream_id]
        if len(app) == 0:
           del self.si_map[app_id]
        self.is_updated = True

    def del_safe(self, stream_id):
        try:
            self.__delete(stream_id)
        except Exception as e:
            logging.exception('stream_info_base_t::del_safe:%s', str(e))

    def __delete(self, stream_id):
        logging.info('stream_info_base_t::__delete stream_id:%s ', str(stream_id))
        for (app,val_map) in self.si_map.items():
            if val_map.has_key(stream_id):
                del val_map[stream_id]
                break
        self.is_updated = True

    def del_by_alias_safe(self, app_id_str, alias):
        try:
            self.__del_by_alias(app_id_str, alias)
            self.is_updated = True
        except Exception as e:
            logging.exception('stream_info_base_t::del_by_alias_safe: %s', str(e))

    def __del_by_alias(self, app_id_str, alias):
        stream_list = []
        if not self.si_map.has_key(app_id_str) or alias == None:
            return
        val_map = self.si_map.get(app_id_str)

        for (stream_id, sip) in val_map.items():
            if sip.alias == alias:
                stream_list.append(stream_id)

        for stream_id in stream_list:
            del val_map[stream_id]

 


    def update_safe(self, stream_id, update_time):
        try:
            self.__update(str(stream_id), int(update_time))
            self.is_updated = True
        except Exception as e:
            logging.exception('stream_info_base_t::update_safe: %s', str(e))

    def __update(self, stream_id32, update_time):
        #logging.info('__update, si_map:%s', str(self.si_map))
        for (app,val_map) in self.si_map.items():
             if val_map.has_key(stream_id32):
                  if update_time != None and update_time != 0:
                      val_map[stream_id32].update_time = update_time
                  else:
                      val_map[stream_id32].update_time = int(time.time())
                  return
        return
    
    def to_json(self, app_id_str, is_by_stream_id = True):
        ret_d = {}
        ret = []
        #logging.info('to_json: app_id_str:%s si_map:%s', app_id_str ,str(self.si_map))
        for (app,val_map) in self.si_map.items():
            if app == app_id_str:
                for (key_sid, sip) in val_map.items():
                    if True == is_by_stream_id:
                        ret.append({'stream_id':key_sid, 'last_update_time':sip.update_time})
                    else:
                        ret.append({'alias': sip.alias, 'last_update_time':sip.update_time})
            else:
                pass
        #ret_d['version'] = '1.0'
        ret_d['error_code'] = '0'
        if True == is_by_stream_id:
            ret_d['stream_id_list'] = ret
        else:
            ret_d['alias_list'] = ret
        return  ujson.encode(ret_d)


