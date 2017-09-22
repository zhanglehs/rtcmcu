from xml.etree import ElementTree
import sys

filename = sys.argv[1]
tracker_ip = sys.argv[2]
tracker_port = sys.argv[3]

conf = ElementTree.parse(filename)
node = conf.find("tracker")
node.attrib["tracker_ip"] = tracker_ip
node.attrib["tracker_port"] = tracker_port

conf.write(filename, encoding="utf-8")  


