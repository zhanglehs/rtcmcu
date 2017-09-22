#include $(XM_SERVER_LIB_SOURCE_PATH)/makefile.incl
include ./makefile.incl
SOURCE_DIR:= ..
include ../lib/makefile.incl

OBJS = admin_server.o arguments.o config_info.o flv_sender.o http_server.o\
	   global_var.o http_connect.o levent.o main.o proto.o \
	   rtmp_ant.o rtmp_uri.o xmants.o ant.o mms2flv_ant.o \
	   ant_transcoder.o ant_transcoder_config.o \
	   diagnostics.o ant_pair.o ant_transcoder_config_item.o 

API_LIB = $(STATIC_LIB_EVENT) $(STATIC_LIB_RTMP) \
		  $(STATIC_LIB_JSONCPP) $(STATIC_LIB_STAT) \
		  $(STATIC_LIB_NET) $(STATIC_LIB_UTILS) \
		  $(STATIC_LIB_XTHREAD) $(STATIC_LIB_LOG4CPP) \
		  $(STATIC_LIB_APPFRAME)

			#$(XM_SERVER_LIB_SOURCE_PATH)/libnet.a \
		  $(XM_SERVER_LIB_SOURCE_PATH)/libutils.a \
		  $(XM_SERVER_LIB_SOURCE_PATH)/libappframe.a \
		  $(XM_SERVER_LIB_SOURCE_PATH)/libxthread.a \
		  $(XM_SERVER_LIB_SOURCE_PATH)/libjson-c++.a \
		  $(XM_SERVER_LIB_SOURCE_PATH)/librtmp.a \
		  $(LOG4CPLUS_LIB) \
		  $(LIBEVENT_LIB) -lpthread -lrt -luuid

MACRO_DEF = -DLOGGER

TARGET=xm_ants

API_INCLUDES:=

.PHONEY = all

all: $(TARGET)

%.o:%.cpp
	g++ $(CXXFLAGS) -c $^ -o $@ 

$(API_LIB):
	@cd $(DIR_LIB); make static-librtmp;
	@cd $(DIR_LIB); make static-libevent;
	make -C $(DIR_LIB_NET)
	make -C $(DIR_LIB_UTILS)
	make -C $(DIR_LIB_JSONCPP)
	make -C $(DIR_LIB_STAT)
	make -C $(DIR_LIB_APPFRAME)
	make -C $(DIR_LIB_XTHREAD)

clean-lib:
	@cd $(DIR_LIB); make clean-librtmp;
	@cd $(DIR_LIB); make clean-libevent;
	make -C $(DIR_LIB_NET) clean
	make -C $(DIR_LIB_UTILS) clean
	make -C $(DIR_LIB_JSONCPP) clean
	make -C $(DIR_LIB_STAT) clean
	make -C $(DIR_LIB_APPFRAME) clean
	make -C $(DIR_LIB_XTHREAD) clean

$(TARGET): $(OBJS) $(API_LIB) 
	$(CXX) $(CXXFLAGS) $(MACRO_DEF) -o $@ $^ -lssl -lcrypto -lpthread -lrt -luuid -ldl -lz 
	@echo
	@echo -e "\033[32mMake xm_ants successfully. \033[0m"
	@echo

clean:
	$(RM) $(OBJS) $(TARGET)
	$(RM) $(TARGET).pid
	$(RM) logs/*

cleanall: clean clean-lib

cleanxmode:
	@chmod -x *.cpp *.h Makefile
