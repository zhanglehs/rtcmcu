DIR_LIB_JSONC = "json-c-0.11"
DIR_LIB_EVENT = "libevent-1.4.14b-stable"
DIR_LIB_PROTOBUF = "protobuf-3.2.0"
DIR_LIB_SYSSTAT = "sysstat-11.1.6"
DIR_LIB_LOG4CPLUS = "log4cplus-1.1.2"

THIRD_PARTY_DIR = "$(shell pwd)"

all: jsonc libevent protobuf log4cplus
	echo build $^

jsonc:
	if [ ! -d $(DIR_LIB_JSONC) ]; then \
		tar xzvf $(DIR_LIB_JSONC).tar.gz; \
		cd $(DIR_LIB_JSONC); \
		sh autogen.sh; \
		./configure; \
		make; \
	fi; \

libevent:
	if [ ! -d $(DIR_LIB_EVENT) ]; then \
		tar xzvf $(DIR_LIB_EVENT).tar.gz; \
		cd $(DIR_LIB_EVENT); \
		./configure --prefix=$(THIRD_PARTY_DIR)/$(DIR_LIB_EVENT)/out; \
		make; \
		make install; \
		cd $(THIRD_PARTY_DIR); \
	fi; \

protobuf:
	if [ ! -d $(DIR_LIB_PROTOBUF) ]; then \
		tar xzvf $(DIR_LIB_PROTOBUF).tar.gz; \
		cd $(DIR_LIB_PROTOBUF); \
		./configure --prefix=$(THIRD_PARTY_DIR)/$(DIR_LIB_PROTOBUF)/out; \
		make; \
		make install; \
		cd $(THIRD_PARTY_DIR); \
	fi; \
	
sysstat:
	if [ ! -d $(DIR_LIB_SYSSTAT) ]; then \
		tar xzvf $(DIR_LIB_SYSSTAT).tar.gz; \
		cd $(DIR_LIB_SYSSTAT); \
		./configure; \
		make; \
		mkdir -p out/include/sysstat; \
		mkdir -p out/lib; \
		cp librdsensors.a out/lib; \
		cp librdstats.a out/lib; \
		cp librdstats_light.a out/lib; \
		cp libsyscom.a out/lib; \
		cp common.h out/include/sysstat; \
		cp rd_stats.h out/include/sysstat; \
		cp pidstat.h out/include/sysstat; \
		cd $(THIRD_PARTY_DIR); \
	fi; \

log4cplus:
	if [ ! -d $(DIR_LIB_LOG4CPLUS) ]; then \
		tar xzvf $(DIR_LIB_LOG4CPLUS).tar.gz; \
		cd $(DIR_LIB_LOG4CPLUS); \
		./configure --prefix=$(THIRD_PARTY_DIR)/$(DIR_LIB_LOG4CPLUS)/out; \
		make; \
		make install; \
		cd $(THIRD_PARTY_DIR); \
	fi; \
