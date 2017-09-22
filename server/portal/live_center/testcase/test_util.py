import sys

sys.path.append('../')
import lutil
import lctrl_conf

def test_get_host_ip():
    try:
        ret = lutil.get_host_ip('10.4.4.0')
        assert(ret == '10.4.4.0')

        ret = lutil.get_host_ip('')
        assert(len(ret) == 0)

        ret = lutil.get_host_ip('127.0.1')
        assert(len(ret) > 0)

        ret = lutil.get_host_ip('www.youku.com')
        assert(len(ret) > 0)

        ret = lutil.get_host_ip('wwwyoukucom2')
        assert(len(ret) == 0)
    except:
        assert(False)

def test_create_client_sock():
    ret = lutil.create_client_sock('127.0.0.1', '10a')
    assert(None == ret)

    ret = lutil.create_client_sock('127.0.0', '10')
    assert(None == ret)

def test_token():
    curr_time = 1234567
    stream_id = 9876543
    user_id_str = '123'
    secret = lctrl_conf.up_sche_secret
    token = lutil.make_token(curr_time, stream_id, user_id_str, '', secret)
    assert(32 == len(token.strip()))


    result = lutil.check_token(curr_time, stream_id, user_id_str, '', secret, 10, 'zdf')
    assert(not result)

    # timeout
    result = lutil.check_token(curr_time + 11, stream_id, user_id_str, '', secret, 10, token)
    assert(not result)

    result = lutil.check_token(curr_time, stream_id, user_id_str, '', secret, 10, token)
    assert(result)

def check_token(curr_time, d, max_timeout):
    ret_token = lutil.make_token(d["curr_time"], d["stream_id"], d["para1_str"],
                                 d["para2_str"], d["private_key"])
    assert(ret_token == d["token"])
    return lutil.check_token(curr_time, d["stream_id"], d["para1_str"],
                             d["para2_str"], d["private_key"], max_timeout,
                             d["token"])


def test_token_base():
    d = {}
    d["curr_time"] = 9897
    d["stream_id"] = 89
    d["private_key"] = "01234567890123456789012345678901"
    d["para1_str"] = "Mozilla/5.0 (MSIE 9.0; Windows NT 6.1; Trident/5.0)"
    d["para2_str"] = "10.11.11.68"
    d["token"] = "000026a9b2a018319b93f113524544ff"

    curr_time = d["curr_time"]
    ret = check_token(curr_time, d, 10)
    assert(ret)
    ret = check_token(curr_time + 11, d, 10)
    assert(not ret)

    d = {}
    d["curr_time"] = 9090898990
    d["stream_id"] = 938
    d["private_key"] = "9876543219"
    d["para1_str"] = ""
    d["para2_str"]  = "0.0.0.0"
    d["token"] = "1ddc1c2eca5db3466e31ff584e640c5d"

    timeout = 0xFFFFFFFFF
    curr_time = d["curr_time"]
    ret = check_token(curr_time, d, timeout)
    assert(ret)
    ret = check_token(curr_time + timeout + 1, d, timeout)
    assert(not ret)

    d = {}
    d["curr_time"] = 9090898990
    d["stream_id"] = 938
    d["private_key"] = '123123123123123123123123123123123123123123123123'
    d["para1_str"] = "Mozilla/5.0 (MSIE 9.0; Windows NT 6.1; Trident/5.0)"
    d["para2_str"] = "0.0.0.0"
    d["token"] = '1ddc1c2ef1b34a1cfffd8967d2cd565b'
    curr_time = d["curr_time"]

    timeout = 0xFFFFFFFFF
    ret = check_token(curr_time, d, timeout)
    assert(ret)
    ret = check_token(curr_time + timeout + 1, d, timeout)
    assert(not ret)

def test_token_fail():
    token = "0123"
    ret = lutil.check_token(0, 0, "", "", "", 10, token)
    assert(not ret)

    token = "z1234567890123456789012345678901"
    ret = lutil.check_token(0, 0, "", "", "", 10, token)
    assert(not ret)


if __name__ == "__main__":
    test_create_client_sock()
    test_token()
    test_token_base()
    test_token_fail()
    test_get_host_ip()
