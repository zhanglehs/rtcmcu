#coding:utf-8 

'''Defines a constant class implements the constant function'''
class _const(object):
    class ConstError(TypeError): 
        pass
    
    def __setattr__(self, name, value):
        if self.__dict__.has_key(name):
            raise self.ConstError, "Cant rebind const(%s)" % name
        self.__dict__[name] = value

    def __delattr__(self, name):
        if name in self.__dict__:
            raise self.ConstError, "Cant unbind const(%s)" % name
        raise NameError, name

import sys
sys.modules[__name__] = _const()

