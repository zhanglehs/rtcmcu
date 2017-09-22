#coding:utf-8
import redis
import ujson
import logging
import time
import cPickle

db_opt = None
db_alias_stream_table = None

HASH_TABLE_NAME = 'stream.db.v3.1'
ALIAS_STREAM_TABLE_NAME = 'alias_stream.db.v3.1'
STREAM_ID_KEY_NAME = 'stream_id'

STREAM_ID_BASE_VALUE = 10000
STREAM_ID_MAX_VALUE = 2147483647 #Integer.MAX_VALUE

class RedisHelper:
    def __init__(self, db_host, db_port, db_idx, table_name, sock_timeout = 1.0):
        self.db_idx = db_idx
        self.table_name = table_name
        self.stream_id_last = STREAM_ID_BASE_VALUE

        p = redis.ConnectionPool(host = db_host,
                                 port = db_port,
                                 db = db_idx,
                                 socket_timeout = sock_timeout)
        self.conn_pool = p
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        r.ping()

    def load_all_kv_raw(self):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        return r.hgetall(self.table_name)

    def load_all_kv(self):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        all_kv = r.hgetall(self.table_name)
        ret = {}
        for k in all_kv:
            ret[k] = ujson.decode(all_kv[k])
	    #ret[k] = cPickle.loads(all_kv[k])
        return ret

    def gen_unique_id(self):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        if None == r.get(STREAM_ID_KEY_NAME):
            r.set(STREAM_ID_KEY_NAME, STREAM_ID_BASE_VALUE)
        temp = int(r.incr(STREAM_ID_KEY_NAME, 1))
        if temp >= STREAM_ID_MAX_VALUE:
            r.set(STREAM_ID_KEY_NAME, STREAM_ID_BASE_VALUE)
            temp = STREAM_ID_BASE_VALUE
        self.stream_id_last = temp
        logging.debug('gen_unique_id stream_id_last:%s', str(self.stream_id_last))
        return temp

    def get_cur_stream_id(self):
        return self.stream_id_last

    def set_stream_id_ex(self, stream_id):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        r.set(STREAM_ID_KEY_NAME, stream_id)
        self.stream_id_last = stream_id
        return True

    def set_key_value_raw(self, key_str, val):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        r.hset(self.table_name, key_str, val)
        return True

    def set_key_value(self, key_str, val_d):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        val_str = ujson.encode(val_d)
	#val_str = cPickle.dumps(val_d)
        r.hset(self.table_name, key_str, val_str)
        return True

    def get_key_value_raw(self, key_str):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        return r.hget(self.table_name, key_str)

    def get_key_value(self, key_str):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        ret = r.hget(self.table_name, key_str)
        if None != ret:
            ret = ujson.decode(ret)
	    #ret = cPickle.loads(ret)

        return ret

    def del_key_value(self, key_str):
        r = redis.StrictRedis(connection_pool = self.conn_pool)
        r.hdel(self.table_name, key_str)

    def fini(self):
        self.conn_pool.disconnect()


def create_table(db_server_ip, db_server_port, db_server_idx, table_name):
    logging.info("create_table start")
    return RedisHelper(db_server_ip,
            db_server_port,
            db_server_idx,
            table_name,
            2)


def destroy_table(db_helper):
    logging.info("destroy_table start")
    if db_helper == None:
        return
    if not isinstance(db_helper, RedisHelper):
        return 
    db_helper.fini()
    db_helper = None

    logging.info("destroy_table end")



def init_persist(db_server_ip, db_server_port, db_server_idx):
    global db_opt
    global db_alias_stream_table

    logging.info("init_persist()")
    db_opt = RedisHelper(db_server_ip,
                        db_server_port,
                        db_server_idx,
                        HASH_TABLE_NAME, 2)

    db_alias_stream_table = RedisHelper(db_server_ip,
                        db_server_port,
                        db_server_idx,
                        ALIAS_STREAM_TABLE_NAME, 2)



def fini_persist():
    global db_opt
    global db_alias_stream_table
    logging.info("fini_persist()")
    db_opt.fini()
    db_opt = None

    db_alias_stream_table.fini() 
    db_alias_stream_table = None



def load_all_kv():
    return db_opt.load_all_kv()

def gen_stream_id():
    return db_opt.gen_unique_id()

def get_stream_id_last():
    return db_opt.get_cur_stream_id()

def add_key_value(key_str, val_d):
    return db_opt.set_key_value(key_str, val_d)

def del_key_value(key_str):
    db_opt.del_key_value(key_str)

def load_all_alias_stream_info():
    return db_alias_stream_table.load_all_kv()

def add_alias_stream_info(key_str, val):
    return db_alias_stream_table.set_key_value(key_str, val)

def del_alias_stream_info(key_str):
    db_alias_stream_table.del_key_value(key_str)


