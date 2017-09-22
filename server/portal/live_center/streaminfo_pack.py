#coding:utf-8
import time
import datetime
import json
import struct
import logging

from timeval_t import timeval_t

#stream info pack for p2f_inf_stream_cmd_v2 
class StreamInfoPack:
    pack_pat = '!Iqq'
    def __init__(self, stream_id = -1, time_val = None):
        self.stream_id = stream_id
        if time_val == None:
            self.time_val = timeval_t()
        else:
            self.time_val = time_val

    def copy(self, other):
        self.stream_id = other.stream_id
        self.time_val = copy.deepcopy(other.time_val)

    def pack(self):
        #payload = struct.pack(StreamInfoPack.pack_pat, self.stream_id, self.timeval.tv_sec, self.timeval.tv_usec)
        temp_pack_pat = '!I'
        payload = struct.pack(temp_pack_pat, int(self.stream_id))
        payload += self.time_val.pack()
        #logging.info('StreamInfoPack.pack:%s streamid:%s timeval:%s' ,str(payload), str(self.stream_id), self.time_val.to_dict())
        return payload

    def unpack(self, pkg):
        if len(str(pkg)) < struct.calcsize(StreamInfoPack.pack_pat):
            return self
        (self.stream_id, self.time_val.tv_sec, self.time_val.tv_usec) = struct.unpack(StreamInfoPack.pack_pat, pkg)
        return self

    @staticmethod
    def pack_s(stream_id, time_val):
        payload = struct.pack(StreamInfoPack.pack_pat, stream_id, time_val.tv_sec, time_val.tv_usec)
        return payload

    @staticmethod
    def unpack_s(pkg):
        result = StreamInfoPack()
        if len(str(pkg)) < struct.calcsize(StreamInfoPack.pack_pat):
            return result
        (result.stream_id, result.time_val.tv_sec, result.time_val.tv_usec) = struct.unpack(StreamInfoPack.pack_pat, pkg)
        return result 

#stream info list payload pack/unpack
class StreamInfoListPack:
    def __init__(self):
        pass

    def pack(self, stream_info_list):
        payload = ''
        try:
            stream_cnt_pat = '!H'
            payload = struct.pack(stream_cnt_pat, len(stream_info_list))
            for stream_info_pack in stream_info_list:#StreamInfoPack stream_info_pack
                payload += stream_info_pack.pack()
        except ValueError as e:
            logging.exception("StreamInfoListPack find ValueError.err:" + str(e))
            return ''
        except struct.error  as e:
            logging.exception("StreamInfoListPack find struct.error.err:"+ str(e))
            return ''
        except Exception as e:
            logging.exception("StreamInfoListPack find Exception.err:"+ str(e))
            return ''
        return payload

    def unpack(self, pkg):
        return ''

