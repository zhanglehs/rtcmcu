#coding:utf-8
import ConfigParser
import logging
import sys
import ujson

class AppCfgInfo():
    def __init__(self):
        self.max_stream_cnt = None
        self.max_transcoding_cnt = None

class AppCfg():
    def __init__(self, file_name, max_stream_cnt_def, max_transcoding_cnt_def):
        self.__file_name = file_name
        self.__section_name = "app_cfg"
        self.__key_name = "max_stream_cfg"
        self.__app_cfg = {}
        self.__max_stream_cnt_default = max_stream_cnt_def
        self.__max_transcoding_cnt_default = max_transcoding_cnt_def
        self.__json_str = ""

    def set_file_name(self, file_name):
        self.__file_name = file_name

    def load(self, cfg_str):
        try:
           self.__app_cfg.clear()
           cfg_all_str = cfg_str.strip()
           app_arr = cfg_all_str.split(',')
           for app_cfg_item in app_arr:
               key_val_arr = app_cfg_item.split(':')
               if len(key_val_arr) < 2:
                  continue
               temp_trans = self.__max_transcoding_cnt_default
               if len(key_val_arr) >= 3:
                  temp_trans = int(key_val_arr[2])
               self.set_cfg(int(key_val_arr[0]), int(key_val_arr[1]), temp_trans, False)

           print('AppCfg::load ' + self.to_str())
           self.gen_new_json()
           return True
        except IOError as e:
            print("AppCfg::load failed! IOError:%s cfg_str:%s", str(e), str(cfg_str))
        except Exception as e:
            print("AppCfg::load failed! Exception:%s cfg_str:%s", str(e), str(cfg_str))

        return False

    def get_max_cnt(self, app_id):
	if self.__app_cfg.has_key(app_id):
	    item = self.__app_cfg.get(app_id)
	    return (item.max_stream_cnt, item.max_transcoding_cnt)
	return (self.__max_stream_cnt_default, self.__max_transcoding_cnt_default)

    def get_max_stream_cnt(self, app_id):
        if self.__app_cfg.has_key(app_id):
            return self.__app_cfg.get(app_id).max_stream_cnt
        return self.__max_stream_cnt_default

    def get_max_transcoding_cnt(self, app_id): 
        if self.__app_cfg.has_key(app_id):
            ret = self.__app_cfg.get(app_id).max_transcoding_cnt
            if ret == -1:
                ret = sys.maxunicode
            return ret
        return self.__max_transcoding_cnt_default

    def set_cfg(self, app_id, stream_cnt, trans_cnt, is_gen_json = True):
        app_cfg = None
        if self.__app_cfg.has_key(app_id): 
            app_cfg = self.__app_cfg.get(app_id)
        else:
            app_cfg = AppCfgInfo()
            self.__app_cfg[app_id] = app_cfg

        app_cfg.max_stream_cnt = stream_cnt
        app_cfg.max_transcoding_cnt = trans_cnt

        if is_gen_json:
             self.gen_new_json()

    def set_max_stream_cnt(self, app_id, stream_cnt):
        app_cfg = None
        if self.__app_cfg.has_key(app_id): 
            app_cfg = self.__app_cfg.get(app_id)
        else:
            app_cfg = AppCfgInfo()
            app_cfg.max_transcoding_cnt = self.__max_transcoding_cnt_default
            self.__app_cfg[app_id] = app_cfg

        app_cfg.max_stream_cnt = stream_cnt

        self.gen_new_json()

    def set_max_transconding_cnt(self, app_id, transcoding_cnt):
        app_cfg = None
        if self.__app_cfg.has_key(app_id): 
            app_cfg = self.__app_cfg.get(app_id)
        else:
            app_cfg = AppCfgInfo()
            self.__app_cfg[app_id] = app_cfg
            app_cfg.max_stream_cnt = self.__max_stream_cnt_default

        app_cfg.max_transcoding_cnt = transcoding_cnt

        self.gen_new_json()

    def save(self):
        try:
           self.save_none_safe()
        except Exception as e:
           logging.warning('save exception:' + str(e))

          
    def save_none_safe(self):
        cf = ConfigParser.ConfigParser()
        cf.read(self.__file_name)
        str_tmp = self.to_str()
        cf.set(self.__section_name, self.__key_name, str_tmp)
        cf.write(open(self.__file_name, "w")) 

        logging.info('save_none_safe successed! %s', str_tmp)


    def delete_app(self, app_id):
        if self.__app_cfg.has_key(app_id):
            del self.__app_cfg[app_id]
        self.gen_new_json()
        self.save()

    def get_json_str(self):
        return self.__json_str

    def gen_new_json(self):
        lst = []
        for (app_id, aci) in self.__app_cfg.items():
            temp_dict = {}
            temp_dict['app_id'] = app_id
            temp_dict['stream_cnt'] = aci.max_stream_cnt
            temp_dict['trans_cnt'] = aci.max_transcoding_cnt
            lst.append(temp_dict)

        self.__json_str = ujson.encode(lst)
        return self.__json_str


    def __str__(self):
        return self.to_str()


    def to_str(self):
        ret_str = ''
        for (app_id, aci) in self.__app_cfg.items():
            if len(ret_str) > 0:
                ret_str += ','
            str_tmp = '%d:%d:%d' % (app_id, aci.max_stream_cnt, aci.max_transcoding_cnt)
            ret_str += str_tmp
        return ret_str





