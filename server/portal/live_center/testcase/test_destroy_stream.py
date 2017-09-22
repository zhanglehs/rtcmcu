import json
import addr


def test_none_para():
    (ret, _) = addr.call_api("POST", "/destroy_stream")
    assert(ret == 200)

def test_empty_para():
    (ret, data) = addr.call_destroy_stream('', '', '', 1)
    assert(ret == 200)
    assert(len(data) != 0)
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

def test_success():
    (ret, data) = addr.call_destroy_stream('s723', '723', 'r723', 1)
    assert(ret == 200)
    assert(len(data) != 0)
    ret = json.JSONDecoder().decode(data)
    assert(ret['error_code'] != 0)

test_none_para()
test_empty_para()
test_success()
