import json
import addr
import sys
import os

sys.path.append('../')
import lstream


def test_invalid_user_id():
    (_, data) = addr.call_create_stream('no', '-123', 'abc')
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

    (_, data) = addr.call_create_stream('no', '0', 'abc')
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

    val = 0x7FFFFFFF
    (_, data) = addr.call_create_stream('no', str(val), 'abc')
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

    (_, data) = addr.call_create_stream('no', 'a123', 'abc')
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

    (_, data) = addr.call_create_stream('no', '123a', 'abc')
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

    (_, data) = addr.call_create_stream('', '1', 'abc')
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

    (_, data) = addr.call_create_stream('1', '1', '-12')
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

    (_, data) = addr.call_create_stream('0', '1', '12')
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

    (_, data) = addr.call_create_stream('no', '1', 'for_debug_str')
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)


def test_post_none_para():
    (ret, _) = addr.call_api("POST", "/create_stream")
    assert(ret == 200)

def test_not_exist_uri():
    (ret, data) = addr.call_api("POST", "/create_stream123")
    assert(ret == 200)
    assert(len(data) == 0)

def test_empty_para():
    (ret, data) = addr.call_api("POST", "/create_stream?site_user_id=&user_id=&room_id=")
    assert(ret == 200)
    assert(len(data) != 0)
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

def test_empty_room_id():
    (ret, data) = addr.call_create_stream('123', '123', '')

    assert(ret == 200)
    assert(len(data) != 0)
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

def test_get_stream_stat(cs_id):
    addr.url_get_json('/get_stream_stat?stream_id=%d' % (cs_id))
    addr.url_get_json('/get_stream_stat')

    (status, data) = addr.call_api('GET', '/get_stream_stat?stream_id=a')
    assert(400 == status)
    assert(len(data) == 0)

def test_successeed_create_stream():
    (ret, data) = addr.call_create_stream_v2('9783495', '978349', '9783496', '1.0', 'fc')

    assert(ret == 200)
    assert(len(data) != 0)
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] == 0)
    assert(type(ret['cs_id']) == type(1))

    test_get_stream_stat(ret['cs_id'])

    assert(type(ret['cs_para']) == type({}))
    assert(type(ret['upload_url']) == type(u''))
    assert(type(ret['ps_list']) == type([]))
    assert(type(ret['ps_list'][0]['ps_id']) == type(1))

    assert(type(ret['ps_list'][0]['ps_info']) == type(u''))
    ps_url = ret['ps_list'][0]['ps_url']
    assert(type(ps_url) == type([]))
    assert(ps_url[0]["type"] == "flv")
    assert(type(ps_url[0]["url"]) == type(u""))
    assert('' != ps_url[0]["url"].strip())

    assert(ps_url[1]["type"] == "ts")
    assert(type(ps_url[1]["url"]) == type(u""))
    assert('' != ps_url[1]["url"].strip())

    assert(ps_url[2]["type"] == "flv_v2")
    assert(type(ps_url[2]["url"]) == type(u""))
    assert('' != ps_url[2]["url"].strip())

    (ret, data) = addr.call_destroy_stream('9783495', '978349', '9783496', str(ret['cs_id']))
    assert(200 == ret)

def test_recreate_stream():
    (ret, data) = addr.call_create_stream('4123', '4123', '4123')
    assert(ret == 200)
    assert(len(data) != 0)
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] == 0)
    assert(type(ret['cs_id']) == type(1))

    (ret2, data2) = addr.call_create_stream('4123', '4123', '4123')
    assert(ret2 == 200)
    assert(len(data2) != 0)
    ret2 = json.JSONDecoder().decode(data2)
    assert(ret2['error_code'] == 0)
    assert(ret2['cs_id'] != ret['cs_id'])

    (ret, data) = addr.call_destroy_stream('4123', '4123', '4123', str(ret2['cs_id']))
    assert(200 == ret)


def test_fails_to_create_stream():
    (ret, data) = addr.call_create_stream('s123', 'invalid_user_id', "-197")
    assert(ret == 200)
    assert(len(data) != 0)
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

def test_fails_to_create_stream_when_close_db():
    os.system('sh redis_init.sh stop')
    try:
        (ret, data) = addr.call_create_stream('1123', '1123', "1123")
        assert(ret == 200)
        assert(len(data) != 0)
        ret = json.JSONDecoder().decode(data)
        assert(ret['error_code'] != 0)
    finally:
        os.system('sh redis_init.sh start')

def test_create_many_stream():
    result = {}
    min_uid = 10001
    max_uid = min_uid + lstream.MAX_STREAM_NUM
    for i in range(min_uid, max_uid):
        (ret, data) = addr.call_create_stream(str(i), str(i), str(i))
        assert(ret == 200)
        assert(len(data) != 0)
        ret = json.JSONDecoder().decode(data)
        assert(ret['error_code'] == 0)
        result[i] = ret['cs_id']


    for i in range(max_uid + 100, max_uid + 100 + 10):
        (ret, data) = addr.call_create_stream(str(i), str(i), str(i))
        assert(ret == 200)
        assert(len(data) != 0)
        ret = json.JSONDecoder().decode(data)
        assert(ret['error_code'] != 0)

    for i in range(min_uid, max_uid):
        (ret, data) = addr.call_destroy_stream(str(i), str(i), str(i), result[i])
        assert(ret == 200)
        assert(len(data) != 0)


if __name__ == "__main__":
    test_invalid_user_id()
    test_post_none_para()
    test_not_exist_uri()
    test_empty_para()
    test_successeed_create_stream()
    test_recreate_stream()
    test_fails_to_create_stream()
    test_empty_room_id()
    test_create_many_stream()
    test_fails_to_create_stream_when_close_db()
