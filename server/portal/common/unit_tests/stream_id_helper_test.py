#coding:utf-8 

import sys
import os

def cur_file_parent_dir():
    return os.path.dirname(cur_file_dir())
 
def cur_file_dir():
    path = os.path.realpath(sys.path[0]) 
    if os.path.isdir(path):
        return path
    elif os.path.isfile(path):
        return os.path.dirname(path)

code_dir = cur_file_parent_dir()
print(code_dir)
sys.path.append(code_dir)

from stream_id_helper import StreamIDHelper

sid_helper = StreamIDHelper(1)
s1,s2,s3 = sid_helper.encode_all()
print('encode HD s1=%s' % s1)
print('encode SD s2=%s' % s2)
print('encode SMOONTH s3=%s' % s3)

result,stream_id,definition_type,ver = sid_helper.decode(s1)
print('decode result:%s stream_id:%u, definition_type:%u, ver=%u' % (str(result), stream_id, definition_type, ver))
result,stream_id,definition_type,ver = sid_helper.decode(s2)
print('decode result:%s stream_id:%u, definition_type:%u, ver=%u' % (str(result), stream_id, definition_type, ver))
result,stream_id,definition_type,ver = sid_helper.decode(s3)
print('decode result:%s stream_id:%u, definition_type:%u, ver=%u' % (str(result), stream_id, definition_type, ver))