from xml.etree import ElementTree
import sys

filename = sys.argv[1]
lctrl_ip = sys.argv[2]
lctrl_port = sys.argv[3]

conf = ElementTree.parse(filename)
portal = conf.find("portal")
portal.attrib["remote_host"] = lctrl_ip
portal.attrib["remote_port"] = lctrl_port

conf.write(filename, encoding="utf-8")  


