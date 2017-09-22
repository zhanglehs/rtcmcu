import logging
import struct
import os
import time
import sys

def cur_file_parent_dir():
    return os.path.dirname(cur_file_dir())
 
def cur_file_dir():
    path = os.path.realpath(sys.path[0]) 
    if os.path.isdir(path):
        return path
    elif os.path.isfile(path):
        return os.path.dirname(path)

try:
    sys.path.append(cur_file_parent_dir())
except:
    pass

from streaminfo_pack import StreamInfoPack 

def test_pack():
    sip = StreamInfoPack(1000)
    pgk = sip.pack()
    
    sip2 = StreamInfoPack()
    sip2.unpack(pgk)

    assert sip.stream_id == sip2.stream_id and sip.timeval.equalto(sip2.timeval) 


def test_copy():
    sip = StreamInfoPack(2000)
    pgk = sip.pack()
    sip2 = StreamInfoPack()
    sip2.copy(sip)
    assert sip.stream_id == sip2.stream_id and sip.timeval == sip2.timeval 

test_pack()
test_copy()

