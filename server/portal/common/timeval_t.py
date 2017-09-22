#coding:utf-8 
import time
import datetime
import ujson
import struct

time_val_pack_pat = '!qq'
#timeval 
class timeval_t:
     def __init__(self):
         self.tv_sec = 0 
         self.tv_usec = 0
         (self.tv_sec, self.tv_usec) = self.now()

     def __str__(self):
         return "tv_sec:" + str(self.tv_sec) +  " tv_usec:" + str(self.tv_usec)

     def copy(self,other):
         if isinstance(other, timeval_t):
             self.tv_sec = other.tv_sec
             self.tv_usec = other.tv_usec
             return True
         return False

     def equalto(self,other):
         if other != None and isinstance(other, timeval_t):
             return self.tv_sec == other.tv_sec and self.tv_usec == other.tv_usec
         return False

     #def __cmp__(self, other):
     #  notEq = -1
     #  eq = 0
     #  if not isinstance(other, timeval_t):
     #      return notEq
     #  if self.tv_sec == other.tv_sec and self.tv_usec == other.tv_usec:
     #      return eq
     #  return 1


     def to_dict(self):
         d = {}
         d['tv_sec'] = str(self.tv_sec)
         d['tv_usec'] = str(self.tv_usec)
         return d

     def from_dict(self, d):
        try:
            self.tv_sec = int(d['tv_sec'])
            self.tv_usec = int(d['tv_usec'])
        except Exception as e:
            logging.exception("timeval_t::from_dict exception:" + str(e))
            return False
        return True

     def to_json_str(self):
        d = self.to_dict()
        return ujson.encode(d)

     def from_json_str(self, json_str):
        try:
            d = ujson.decode(json_str)
            self.from_dict(d)
            return True
        except e:
            logging.exception("timeval_t::from_json_str exception when decode %s exception:%s", json_str, str(e))

        return False

     @staticmethod
     def from_time(timestamp):
        second = int(timestamp);
        microsecond = int(round(timestamp % second * 1000000,0))
        return (second, microsecond)

     @staticmethod
     def now():
        return timeval_t.from_time(time.time())

     @staticmethod
     def timestamp2str(t):
         ret = time.localtime(int(t))
         return time.strftime('%Y-%m-%d %H:%M:%S', ret)
      
     @staticmethod     
     def str2timestamp(ts_str):
         ret = time.strptime(ts_str, '%Y-%m-%d %H:%M:%S')
         return int(time.mktime(ret))

     def pack(self):
         payload = struct.pack(time_val_pack_pat, self.tv_sec, self.tv_usec)
         return payload

     def unpack(self, pkg):
         pack_size = struct.calcsize(time_val_pack_pat)
         tmp_pkg = pkg[0:pack_size]
         (self.tv_sec, self.tv_usec) = struct.unpack(time_val_pack_pat, tmp_pkg)
         return self

     @staticmethod
     def unpack_s(pkg):
         pack_size = struct.calcsize(time_val_pack_pat)
         tmp_pkg = pkg[0:pack_size]
         tmp = timeval_t()
         (tmp.tv_sec, tmp.tv_usec) = struct.unpack(time_val_pack_pat, tmp_pkg)
         return tmp
