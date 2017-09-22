import time

def safe_get(self, d, key, default_val, is_str = False):
    ret_val = default_val
    try:
        if is_str:
            ret_val = str(d[key])
        else:
            ret_val = int(d[key])
    except Exception as e:
         pass
    return ret_val

def pack_key(app_id, alias):
    if app_id == None or app_id == '' or alias == None or alias == '':
        return ''
    return str(app_id) + '___' + str(alias)

def unpack_key(keys):
    ret = (False, 0, '')
    if keys == None:
        return ret
    val_arr = str(keys).split('___')
    if len(val_arr) < 2:
        return ret

    if not str(val_arr[0]).isdigit():
        return ret
    return (True, val_arr[0], val_arr[1])



def get_current_ms():
    return int(time.time() * 1000)

def get_current_tick():
    return int(time.time() * 1000)



