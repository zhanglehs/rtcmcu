#coding:utf-8 
'''
@file stream_id_helper.py
@brief The 32 bit streamid and 128 bit streamid swap helper.
       It can be used to generate flow ID ultra clear, HD and SD three types.
@author renzelong
<pre><b>copyright: Youku</b></pre>
<pre><b>email: </b>renzelong@youku.com</pre>
<pre><b>company: </b>http://www.youku.com</pre>
<pre><b>All rights reserved.</b></pre>
@date 2014/08/14
@see  ./unit_tests/stream_id_helper_test.py \n
'''
import const

#const define
const.VER = 1 #128 bit stream_id ver
const.HEX = 16
const.DECIMAL = 10
const.STREAM_ID_128BITS_LEN = 32 #The 128 bit streamid character indicates the length


'''
Definition of type definition.
'''
class DefiType:
    ORGIN = 0 #orgin
    SMOOTH = 1 #smooth
    SD = 2 #standard definition
    HD = 3 #high definition
'''
The 32 bit streamid and 128 bit streamid swap helper.
It can be used to generate flow ID ultra clear, HD and SD three types.
'''
class StreamIDHelper:
    def __init__(self, stream_id_32bits = 0):
        self.__stream_id_32bits = stream_id_32bits
        self.__ver = const.VER
        
    def __hex2int(self, string_num):
        return int(string_num.upper(), const.HEX)

    def __hex2long(self, string_num):
        return long(string_num.upper(), const.HEX)

    #return:none
    def set_stream_id(selfk, stream_id_32bits):
        self.__stream_id_32bits = stream_id_32bits

    def get_defi_type_by_resolution(self, width):
        definition_type = DefiType.HD
        if width >= 1280:
            definition_type = DefiType.HD
        elif  width >= 960:
            definition_type = DefiType.SD
        else:
            definition_type = DefiType.SMOOTH
        return definition_type

    def get_defi_type_by_height(self, height):
        definition_type = DefiType.HD
        if height >= 720:
            definition_type = DefiType.HD
        elif  height >= 540:
            definition_type = DefiType.SD
        else:
            definition_type = DefiType.SMOOTH
        return definition_type



    #return:stream_string(32bytes)
    def encode(self, definition_type = DefiType.HD):
        if  DefiType.ORGIN == definition_type:
           val = '%01X%013X%02X%016X' % ( 0, 0, definition_type, self.__stream_id_32bits )
        else:
           val = '%01X%013X%02X%016X' % ( self.__ver & 0x0F, 0, definition_type, self.__stream_id_32bits )
        return val

     #return:stream_string(32bytes)
    def encode_by_resolution(self, width):
        definition_type = DefiType.HD
        if width >= 1280:
            definition_type = DefiType.HD
        elif  width >= 960:
            definition_type = DefiType.SD
        else:
            definition_type = DefiType.SMOOTH
        #val = '%016X%02X%013X%01X' % ( self.__stream_id_32bits, definition_type, 0, self.__ver & 0x0F)
        val = '%01X%013X%02X%016X' % ( self.__ver & 0x0F, 0, definition_type, self.__stream_id_32bits )
        return val
   
    def encode_by_height(self, height):
        definition_type = DefiType.HD
        if height >= 720:
            definition_type = DefiType.HD
        elif  height >= 540:
            definition_type = DefiType.SD
        else:
            definition_type = DefiType.SMOOTH
        #val = '%016X%02X%013X%01X' % ( self.__stream_id_32bits, definition_type, 0, self.__ver & 0x0F)
        val = '%01X%013X%02X%016X' % ( self.__ver & 0x0F, 0, definition_type, self.__stream_id_32bits )
        return val



    #return:stream_hd, stream_sd, stream_smooth
    def encode_all(self):
        return self.encode(DefiType.HD), self.encode(DefiType.SD), self.encode(DefiType.SMOOTH)

    #return: result,stream_id,definition_type,ver
    def decode(self, stream_id_128bits):
        if len(str(stream_id_128bits)) != const.STREAM_ID_128BITS_LEN:
            return False,0,0,0
        '''
        stream_id_ret = self.__hex2int(stream_id_128bits[0:16])
        definition_type = self.__hex2int(stream_id_128bits[16:18])
        other =  self.__hex2long(stream_id_128bits[18:31])
        ver = self.__hex2int(stream_id_128bits[-1:])
        return True,stream_id_ret,definition_type,ver
        '''
        stream_id_ret = self.__hex2int(stream_id_128bits[-16:])
        definition_type = self.__hex2int(stream_id_128bits[15:17])
        other =  self.__hex2long(stream_id_128bits[1:14])
        ver = self.__hex2int(stream_id_128bits[0])
        return True,stream_id_ret,definition_type,ver

 
   
