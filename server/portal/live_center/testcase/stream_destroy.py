import addr
import json

ip = '127.0.0.1'
port = 9090
my_id = '10002'
sid = 2
(_, data) = addr.destroy_stream_impl(ip, port, my_id, my_id, my_id, sid, '')
print(data)
ret = json.JSONDecoder().decode(data)
assert(ret['error_code'] == 0)
