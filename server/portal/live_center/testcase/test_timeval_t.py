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


import timeval_t

def test_now():
    tmp = timeval_t.timeval_t()
    print(tmp.to_str)

def test_pack():
    tmp = timeval_t.timeval_t()
    pkg = tmp.pack()

    tmp2 = timeval_t.timeval_t()
    tmp2.unpack(pkg)

    assert tmp.equalto(tmp2)

    tmp3 = timeval_t.timeval_t.unpack_s(pkg)
    assert tmp.equalto(tmp3)

def test_to_dict():
    tmp = timeval_t.timeval_t()
    dic = tmp.to_dict()
    print(dic);

    tmp2 = timeval_t.timeval_t()
    tmp2.from_dict(dic)
    print(tmp2.to_dict())
    assert tmp.equalto(tmp2)

def test_to_json_str():
    tmp = timeval_t.timeval_t()
    print(tmp.to_json_str())

test_now()
test_pack()
test_to_dict()
test_to_json_str()

     
