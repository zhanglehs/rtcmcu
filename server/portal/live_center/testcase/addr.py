import socket
import httplib
import json
import urllib

gip = "127.0.0.1"
gport = 9090

bin_ip = "127.0.0.1"
bin_port = 9091

def generate_host():
    return "%s:%d" % (gip, gport)

def generate_url(uri):
    return "http://%s:%d%s" % (gip, gport, uri)

def create_conn_impl(my_ip, my_port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
    sock.connect((my_ip, my_port))
    return sock


def create_connection():
    return create_conn_impl(gip, gport)

def create_bin_conn():
    return create_conn_impl(bin_ip, bin_port)

def call_api(method, uri, host = None):
    if None == host:
        host = generate_host()
    conn = httplib.HTTPConnection(host)
    conn.request(method, uri)
    ret = conn.getresponse()
    data = ret.read()
    return (ret.status, data)


def create_stream_impl(ip, port, site_user_id_str, user_id_str, room_id_str, client_ver, client_type = None):
    uri = "/create_stream?site_user_id=%s&user_id=%s&room_id=%s"
    uri %= (site_user_id_str, user_id_str, room_id_str)
    if None != client_ver and '' != client_ver.strip():
        uri += '&client_version=%s' % (client_ver.strip())
    if None != client_type and '' != client_type.strip():
        uri += '&client_type=%s' % (client_type.strip())
    host = '%s:%d' % (ip, port)
    return call_api("POST", uri, host)

def destroy_stream_impl(ip, port, site_user_id_str, user_id_str, room_id_str, stream_id_str, client_ver = ''):
    uri = "/destroy_stream?site_user_id=%s&user_id=%s&room_id=%s&stream_id=%s"
    uri %= (site_user_id_str, user_id_str, room_id_str, stream_id_str)
    if None != client_ver and '' != client_ver.strip():
        uri += '&client_version=%s' % (client_ver)
    host = '%s:%d' % (ip, port)
    return call_api("POST", uri, host)

def call_create_stream_v2(site_user_id_str, user_id_str, room_id_str, client_ver, client_type = None):
    return create_stream_impl(gip, gport, site_user_id_str, user_id_str, room_id_str, client_ver, client_type)

def call_create_stream(site_user_id_str, user_id_str, room_id_str):
    return create_stream_impl(gip, gport, site_user_id_str, user_id_str, room_id_str, '')

def call_destroy_stream(site_user_id_str, user_id_str, room_id_str, stream_id_str):
    return destroy_stream_impl(gip, gport, site_user_id_str, user_id_str, room_id_str, stream_id_str)

def call_get_room_stream(room_id_str):
    uri = "/get_room_stream?room_id=%s"
    uri %= (room_id_str)
    return call_api("GET", uri)

def create_stream(site_user_id_str, user_id_str, room_id_str):
    (_, data) = call_create_stream(site_user_id_str, user_id_str, room_id_str)
    ret_json = json.JSONDecoder().decode(data)
    return ret_json['cs_id']

def destroy_stream(site_user_id_str, user_id_str, room_id_str, stream_id_str):
    (_, data) = call_destroy_stream(site_user_id_str, user_id_str, room_id_str, stream_id_str)
    ret_json = json.JSONDecoder().decode(data)
    return ret_json['error_code']



def url_get_json(uri):
    url = generate_url(uri)
    f = urllib.urlopen(url)
    ret = f.read()

    try:
        json.JSONDecoder().decode(ret)
    except:
        print(uri, ret)
        assert(False)
