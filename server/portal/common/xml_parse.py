# -*- coding:utf-8 -*-
from  xml.dom import  minidom, Node

class BaseParseXML():
    def __init__(self, file_name):
        self.__file_name = file_name
        self.__doc = None
        self.__root = None
        self.__get_root()

    def __get_root(self):
        try:
            self.__doc = minidom.parse(self.__file_name) 
            self.__root = self.__doc.documentElement
        except Exception as e:
            print('__get_root err! e:%s' % str(e))
        return self.__root
    
    def getText(self,node):
        if node.nodeType == Node.TEXT_NODE :
            return node.nodeValue
        else: 
            return ""

    #return list: [{},{}]
    def handle_catalog_attr_list(self, cata_name):
        temp = self.__doc.getElementsByTagName(cata_name)[0]
        list = []
        #direct attributes
        if not temp.hasChildNodes() and temp.hasAttributes():
             attr_list = {}
             node_map =  temp.attributes
             for m in range(0, node_map.length, 1):
                 attr = node_map.item(m)
                 attr_list[attr.name.encode('utf-8','ignore')] = attr.value.encode('utf-8','ignore')
             list.append(attr_list)
             print('cata_name:%s list:%s' % (cata_name,list))
             return list

        #contain child
        for child in temp.childNodes:
             if child.nodeType != Node.ELEMENT_NODE:
                 continue
             if not child.hasAttributes():
                 continue
             node_map =  child.attributes
             attr_list = {}
             for m in range(0, node_map.length, 1):
                 attr = node_map.item(m)
                 attr_list[attr.name.encode('utf-8','ignore')] = attr.value.encode('utf-8','ignore')
             list.append(attr_list)
        print('cata_name:%s list:%s' % (cata_name,list))
        return list

      #return list: [{},{}]
    def handle_catalog_list(self, cata_name, parent_node = None):
        if parent_node == None:
            parent_node = self.__doc
        temp = parent_node.getElementsByTagName(cata_name)[0]
        list = []
        for child in temp.childNodes:
             if child.nodeType != Node.ELEMENT_NODE:
                 continue
             if not child.hasChildNodes():
                 continue

             attr_list = {}
             #for c in child.childNodes:
             # if c.nodeType != c.ELEMENT_NODE:
             #     continue
             attr_list[child.tagName.encode('utf-8','ignore')] = self.getText(child.firstChild).encode('utf-8','ignore')
             list.append(attr_list)
        print('list:%s' % list)
        return list


