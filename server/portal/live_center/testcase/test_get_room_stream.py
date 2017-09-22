import addr
import json

def test_none_para():
    (ret, data) = addr.call_api("GET", "/get_room_stream")
    assert(200 == ret)
    assert(len(data) != 0)
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

def test_empty_para():
    (ret, data) = addr.call_get_room_stream("")
    assert(200 == ret)
    assert(len(data) != 0)
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

def test_not_exist_room():
    (ret, data) = addr.call_get_room_stream("dir")
    assert(200 == ret)
    assert(len(data) != 0)
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] == 0)

def test_create_successfully():
    (ret, data) = addr.call_create_stream('54', '54', "54")
    assert(200 == ret)
    assert(len(data) != 0)
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] == 0)

    (gret, data) = addr.call_get_room_stream('53')
    assert(200 == gret)
    assert(len(data) != 0)
    gret = json.JSONDecoder().decode(data)
    assert(gret['error_code'] == 0)
    assert(len(gret['stream_list']) == 0)

    (gret, data) = addr.call_get_room_stream('54')
    assert(200 == gret)
    assert(len(data) != 0)
    gret = json.JSONDecoder().decode(data)
    assert(gret['error_code'] == 0)
    assert(len(gret['stream_list']) == 1)

    stream = gret['stream_list'][0]
    assert(ret['cs_id'] == stream['cs_id'])
    assert(ret['upload_url'] == stream['upload_url'])
    assert(ret['ps_list'] == stream['ps_list'])

    (ret, data) = addr.call_destroy_stream('54', '54', '54', str(ret['cs_id']))
    assert(200 == ret)

    (gret, data) = addr.call_get_room_stream('54')
    assert(200 == gret)
    assert(len(data) != 0)
    gret = json.JSONDecoder().decode(data)
    assert(gret['error_code'] == 0)
    assert(len(gret['stream_list']) == 0)



if __name__ == "__main__":
    test_none_para()
    test_empty_para()
    test_not_exist_room()
    test_create_successfully()
