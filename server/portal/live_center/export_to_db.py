import lpersist
import logging

class DatabaseInfo():
    def __init__(self, ip, port, idx):
        self.ip = ip
        self.port = port
        self.idx = idx

class ExportToDb():
    def __init__(self):
        self.__si_helper = None
        self.__as_helper = None

    def __del__(self):
        self.__si_helper = None
        self.__as_helper = None

    def open_table(self, db_info, table_name):
        if db_info == None:
            return None
        return lpersist.create_table(db_info.ip, db_info.port, db_info.idx, table_name)

    def close_table(self, tb_obj):
        if tb_obj != None:
            lpersist.destroy_table(tb_obj)
        tb_obj = None

    def open(self, db_info):
        self.__si_helper = self.open_table(db_info, lpersist.HASH_TABLE_NAME)
        self.__as_helper = self.open_table(db_info, lpersist.ALIAS_STREAM_TABLE_NAME)

        return self.__as_helper != None and self.__si_helper != None

    def close(self):
        self.close_table(self.__si_helper)
        self.close_table(self.__as_helper)


    def clear(self, db_obj):
        if db_obj == None:
           return False
        old_dict = db_obj.load_all_kv()
        for key_str in old_dict.keys():
           db_obj.del_key_value(key_str)

        return True

    def clear_all(self):
        self.clear(self.__si_helper)
        self.clear(self.__as_helper)

    def export(self, stream_id, si_map, as_map):
        try:
            logging.info('export stream_id(%s) si_map:%s as_map:%s', str(stream_id), str(si_map), str(as_map))
            return self.__export(stream_id, si_map, as_map)
        except Exception as e:
            logging.exception('ExportToDb::__export exception:%s', str(e))
        return False


    def __export(self, stream_id, si_map, as_map):
        if stream_id == None or si_map == None or as_map == None:
            return False

        self.__si_helper.set_stream_id_ex(stream_id)

        for (key_str, val_d) in si_map.items():
            self.__si_helper.set_key_value(key_str, val_d)

        for (key_str, val_d) in as_map.items():
            self.__as_helper.set_key_value(key_str, val_d)
        return True
