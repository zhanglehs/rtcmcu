#coding:utf-8
import redis
import ujson
import logging
import time


db_opt = None

HASH_TABLE_NAME = 'stream.db.v100'
STREAM_ID_KEY_NAME = 'stream_id22'

STREAM_ID_BASE_VALUE = 10000
STREAM_ID_MAX_VALUE = 2147483647 #Integer.MAX_VALUE

class RedisHelper:
    def __init__(self, db_host, db_port, db_idx, table_name, sock_timeout = 1.0):
        self.db_idx = db_idx
        self.table_name = table_name

        p = redis.ConnectionPool(host = db_host,
                                 port = db_port,
                                 db = db_idx,
                                 socket_timeout = sock_timeout)
        self.conn_pool = p
        self.pubsub = None
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        r.ping()


    def load_all_kv(self):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        all_kv = r.hgetall(self.table_name)
        ret = {}
        for k in all_kv:
            ret[k] = ujson.decode(all_kv[k])
        return ret

    def gen_unique_id(self):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        if None == r.get(STREAM_ID_KEY_NAME):
            r.set(STREAM_ID_KEY_NAME, STREAM_ID_BASE_VALUE)
        temp = int(r.incr(STREAM_ID_KEY_NAME, 1))
        if temp >= STREAM_ID_MAX_VALUE:
            r.set(STREAM_ID_KEY_NAME, STREAM_ID_BASE_VALUE)
            temp = STREAM_ID_BASE_VALUE
        return temp

    def set_key_value(self, key_str, val_d):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        val_str = ujson.encode(val_d)
        r.hset(self.table_name, key_str, val_str)
        return True

    def get_key_value(self, key_str):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        ret = r.hget(self.table_name, key_str)
        if None != ret:
            ret = ujson.decode(ret)

        return ret

    def del_key_value(self, key_str):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        r.hdel(self.table_name, key_str)

    def fini(self):
        if None != self.pubsub:
            self.pubsub.close()
            self.pubsub = None
        self.conn_pool.disconnect()


def init_persist(db_server_ip, db_server_port, db_server_idx):
    global db_opt

    logging.info("init_persist()")
    db_opt = RedisHelper(db_server_ip,
                        db_server_port,
                        db_server_idx,
                        HASH_TABLE_NAME,2)

def fini_persist():
    global db_opt
    logging.info("fini_persist()")
    db_opt.fini()
    db_opt = None

def load_all_kv():
    return db_opt.load_all_kv()

def gen_stream_id():
    return db_opt.gen_unique_id()

def add_key_value(key_str, val_d):
    return db_opt.set_key_value(key_str, val_d)

def del_key_value(key_str):
    db_opt.del_key_value(key_str)



def test_all():
    init_persist('127.0.0.1', 16379, 1)
    for i in range(1, 100):
       add_key_value(str(i), str(i) + ' case_data')

    for i in range(1, 100):
       del_key_value(str(i))

test_all()

    

