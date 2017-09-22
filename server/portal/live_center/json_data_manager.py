#coding:utf-8 
import logging
import time

class JsonDataItem():
   def __init__(self, callback_func, is_chanaged):
       self.callback_func = callback_func
       self.is_chanaged = is_chanaged
       self.json_str = ''

   def to_str(self):
       return 'callback_func:' + str(self.callback_func) + ' is_chanaged:' + str(self.is_chanaged) + ' json_str:' + str(self.json_str)

   def set_flag(self):
       self.is_chanaged = True


   def check_update(self):
       try:
           if self.is_chanaged:
               self.json_str = self.callback_func()
               self.is_chanaged = False
       except Exception as e:
           logging.exception('JsonDataItem::check_update exception:', str(e))

   def get_json_str(self):
       self.check_update()
       #logging.info(self.to_str())
       return self.json_str

class JsonDataManager(object):
   def __init__(self):
       self.__json_data_item = {}
       self.__check_time_last = int(time.time())

   def add(self, json_name, callback_func):
       if None == json_name or '' == json_name or self.__json_data_item.has_key(json_name):
           return False
       jdi = JsonDataItem(callback_func, json_name)
       self.__json_data_item[json_name] = jdi

       return True

   def remove(self, json_name):
       if None == json_name or '' == json_name:
           return

       if self.__json_data_item.has_key(json_name):
           del self.__json_data_item[json_name]

   def clear(self):
       self.__json_data_item.clear()

   def get(self, json_name):
       if None == json_name or '' == json_name or (not self.__json_data_item.has_key(json_name)):
           return None
       return self.__json_data_item.get(json_name).get_json_str()

   def set_flag(self):
       for k, v in self.__json_data_item.items():
           v.set_flag()

   def check(self):
       try:
           self.__check()
       except Exception as e:
           logging.exception('JsonDataManager::check exception:', str(e))

   def __check(self):
       now_ts = int(time.time())
       if now_ts - self.__check_time_last < 50:
           return
       self.__check_time_last = now_ts
       for k, v in self.__json_data_item.items():
           v.check_update()




