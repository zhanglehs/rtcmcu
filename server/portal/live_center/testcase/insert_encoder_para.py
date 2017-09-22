import sys
sys.path.append('../')

import lstream
import lpersist
import json

#db_server_ip = '10.10.69.195'
#db_server_port = 16379
db_server_ip = '10.105.20.67'
db_server_port = 6379
user_id = '323953560'
room_id = '810'

json_str = """
{
"version":"256p_low2",
"vsp":{
          "wgt":320,
          "hgt":240,
          "fInt":666666
          },
"vep":{
          "brt":200,
          "lre":10,
          "vbf":100,
          "vmr":200,
          "rmd":0,
          "kim":10,
          "kima":3,
          "pitr":0,
          "pite":0,
          "mr":4,
          "tl":0,
          "dd":false,
          "qm":10,
          "qma":40,
          "wp":0,
          "sr":0,
          "qc":0.8,
          "mm":0,
          "rf":2,
          "mre":true,
          "bf":0,
          "abf":0,
          "bfp":0,
          "dm":1,
          "pll":false,
          "sc":0,
          "lh":0
          }
}
"""


encoder_db_opt = lpersist.redis_db_t(db_server_ip, db_server_port,
                            lpersist.ENCODER_PARA_DB,
                            lpersist.ENCODER_PARA_TABLE)


si = lstream.stream_info_t()
si.user_id_str = user_id
si.room_id_str = room_id
if len(user_id) > 0:
    key_str = lstream.make_encoder_para_key(si)
else:
    key_str = lstream.make_encoder_para_key_for_room(room_id)

val = json.JSONDecoder().decode(json_str)
ret = encoder_db_opt.add_key_value(key_str, val)
if ret:
    print("succeed for user_id(%s) in room(%s).\n" % (user_id, room_id))
else:
    print("fails for  user_id(%s) in room(%s)!\n" % (user_id, room_id))
