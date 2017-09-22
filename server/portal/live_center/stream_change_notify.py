#coding:utf-8
class IStreamChangeNotify(object):
    """description of class"""
    def on_stream_create(self, stream_info, do_create, is_hd):
        pass
    def on_stream_destroy_notify(self, stream_info, do_destroy):
        pass
    def on_stream_destroy(self, stream_info, do_destroy):
        pass
    def on_receiver_got_data(self, stream_info):
        pass
    def on_receiver_find_data_timeout(self, stream_info):
        pass
    def on_stream_recreate(self, stream_info):
        pass


