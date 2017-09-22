#coding:utf-8
import logging

class StatItem():
      def __init__(self):
	  self.__stream_set = set()
	  self.__nt_set = set()

      def add(self, stream_id, is_nt = 0):
	  try:
	     self.__stream_set.add(stream_id)
	     if 1 == is_nt:
	        self.__nt_set.add(stream_id)
	  except Exception as e:
	     logging.exception('StatItem::add exception ' + str(e))

      def remove(self, stream_id):
	  try:
	     if stream_id in self.__stream_set:
	        self.__stream_set.remove(stream_id)

	     if stream_id in self.__nt_set:
	        self.__nt_set.remove(stream_id)
	  except Exception as e:
	     logging.exception('StatItem::remove exception ' + str(e))

      def count(self):
	  return (len(self.__stream_set), len(self.__nt_set))


class StreamCount():
      def __init__(self):
	  self.__app_stream_map = {} #map<app_id, StatItem>

      def add(self, app_id, stream_id, is_nt):
	  try:
	     obj = None
	     if not self.__app_stream_map.has_key(app_id):
                 obj = StatItem()
	         self.__app_stream_map[app_id] = obj

             else:
	         obj =  self.__app_stream_map.get(app_id)

	     obj.add(stream_id, is_nt)
	  except Exception as e:
	     logging.exception('StreamCount::add exception ' + str(e))

      def remove(self, app_id, stream_id):
	  try:
	     self.__remove(app_id, stream_id)
	  except Exception as e:
             logging.exception('StreamCount::remove exception ' + str(e))

      def __remove(self, app_id, stream_id):
          if not self.__app_stream_map.has_key(app_id):
              return
          obj = self.__app_stream_map.get(app_id)
	  obj.remove(stream_id)
          (s_cnt, trans_cnt) = obj.count()
          if s_cnt <= 0:
	      del self.__app_stream_map[app_id]

      def count(self, app_id):
	  if not self.__app_stream_map.has_key(app_id):
	      return (0, 0)
	  return self.__app_stream_map.get(app_id).count()
	  


     	      
