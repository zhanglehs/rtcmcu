#coding:utf-8
import logging
import time
import ujson

class AliasStreamValue:
    def __init__(self):
        self.app_id = 0
        self.alias = ''
        self.stream_id = 0
        self.last_update_time = int(time.time())

    def __str__(self):
        str_info = ''
        str_info += ('app_id:' + str(self.app_id))
        str_info += ('#alias:' + str(self.alias))
        str_info += ('#stream_id:' + str(self.stream_id))
        str_info += ('#last_update_time:' + str(self.last_update_time))
        return str_info

    def to_dict(self):
        try:
            return self.to_dict_none_safe()
        except Exception as e:
            logging.exception("AliasStreamMap::to_dict exception:" + str(e))
        return ''

    def to_dict_none_safe(self):
        d = {} 
        d['app_id'] = self.app_id
        d['stream_id'] = self.stream_id
        d['alias'] = self.alias 
        d['last_update_time'] = self.last_update_time
        return d

    def from_dict(self, d):
        try:
            self.app_id = int(d['app_id'])
            self.stream_id = int(d['stream_id'])
            self.alias = str(d['alias'])
            self.last_update_time = str(d['last_update_time'])
        except Exception as e:
            logging.exception("AliasStreamMap::from_dict exception:" + str(e))
            return False

        return True

    def to_json_str(self):
        return ujson.encode(self.to_dict())

    def from_json_str(self, json_str):
        try:
            self.from_dict(ujson.decode(json_str))
            return True
        except Exception as e:
            logging.exception("AliasStreamMap::from_json_str exception when decode  %s exception:%s", json_str, str(e)) 
        return False

class AliasStreamMap():
    def __init__(self):
        self.__alas_stream_map = {} #hash_map<alias, AliasStreamValue>


    def __str__(self):
        str_info = ''
        for alias in self.__alas_stream_map:
	    asv = self.__alas_stream_map.get(alias)
            if len(str_info) > 0:
                str_info += '&'
            str_info += str(asv)
        return str_info

    def items(self):
        return self.__alas_stream_map.items()

    def keys(self):
        return self.__alas_stream_map.keys()

    def get_maps(self):
        return self.__alas_stream_map

    def get(self, alias):
        return self.__alas_stream_map.get(alias, None)

    def get_alias_list(self, dst_time):
        stream_id_array = []
        for alias in self.__alas_stream_map:
	   asv = self.__alas_stream_map.get(alias)
           temp_dict = {'alias': asv.alias,'time_dif': (dst_time - int(asv.last_update_time))}
           stream_id_array.append(temp_dict)
        return stream_id_array


    def update_time(self, alias, stream_id, update_time):
        asv = self.__alas_stream_map.get(alias, None)
        if asv == None:
            return False

        if asv.stream_id != stream_id:
            return False

        asv.last_update_time = update_time

        return True


    def add_ex(self, asv):#AliasStreamValue
        if not isinstance(asv, AliasStreamValue):
            return False
        if asv.alias == None or asv.alias == '':
            return False
        self.__alas_stream_map[asv.alias] = asv
        return True


    def add(self, alias, stream_id, app_id):
        try:
            return self.add_none_safe(alias, stream_id, app_id)
        except Exception as e:
            logging.exception('add find exception:%s', str(e))
        return False

    def add_none_safe(self, alias, stream_id, app_id):
        if alias == None or alias == '':
            return None
        new_obj = AliasStreamValue()
        new_obj.alias = alias
        new_obj.app_id = app_id
        new_obj.stream_id = stream_id
        new_obj.last_update_time = int(time.time())
        self.__alas_stream_map[alias] = new_obj
        return new_obj

    def delete(self, alias):
        if alias == None or alias == '':
            return False
        if self.__alas_stream_map.has_key(alias):
            del self.__alas_stream_map[alias]
        return True

    def check_timeout(self, dst_time, timeout_len, timeout_proc):
        try:
            self.check_timeout_none_safe(dst_time, timeout_len, timeout_proc)
        except Exception as e:
            logging.exception("AliasStreamMap::check_timeout find exception:%s", str(e))

    def check_timeout_none_safe(self, dst_time, timeout_len, timeout_proc):
        for alias in self.__alas_stream_map.keys():
            asv = self.__alas_stream_map.get(alias)
            if dst_time - asv.last_update_time >= timeout_len:
	        logging.info('check_timeout_none_safe delete app_id(%s) alias(%s)', str(asv.app_id), str(alias))
		timeout_proc(alias, asv.app_id)
                del self.__alas_stream_map[alias]

    def update(self, alias, stream_id, update_time):
        asv = self.__alas_stream_map.get(alias, None)
        if asv == None:
            return False

        if asv.stream_id == int(stream_id):
            asv.last_update_time = update_time
        else:
            logging.warning("AliasStreamMap::update alias, stream_id_old(%s) != stream_id(%s)", 
                    str(asv.stream_id), str(stream_id))
        return True
        


