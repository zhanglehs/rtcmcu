mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(patsubst %/,%,$(dir $(mkfile_path)))
source_dir  := $(current_dir)/../../../lib
ifeq	($(strip $(XM_SERVER_LIB_SOURCE_PATH)),)
	XM_SERVER_LIB_SOURCE_PATH = $(source_dir)
endif		

CC = g++
CFLAGS = -g -m64 -Wall -fomit-frame-pointer -pipe -DHAVE_SCHED_GETAFFINITY 
CXXFLAGS= -I../../../lib/include $(INC_UTILS) $(INC_REACTOR) -I../cmdserver -I../../common $(INC_PROTOBUF)
include $(XM_SERVER_LIB_SOURCE_PATH)/makefile.incl

THIRD_PARTY_LIB = $(STATIC_LIB_EVENT) $(STATIC_LIB_LOG4CPP) $(STATIC_LIB_PROTOBUF) $(STATIC_LIB_LOG4CPP) $(STATIC_LIB_REACTOR)

RPC_OBJS =  rpc_channel.o rpc_server.o rpc_common.o lookup3.o 

RPC_LIB = librpc.a 
RPC_SERVER_TEST = rpc_server_test
RPC_CLIENT_TEST = rpc_client_test

RPC_SERVER_TEST_OBJS = rpc_server_test.o $(RPC_OBJS) ../cmdserver/libserver.a echo.pb.o tracker.pb.o ../../../lib/utils/buffer.o ../../../lib/utils/memory.o ../../../lib/utils/logging.o  ../../../server/util/log.o ../../../server/util/statfile.o $(THIRD_PARTY_LIB)

RPC_CLIENT_TEST_OBJS = rpc_client_test.o rpc_channel.o echo.pb.o tracker.pb.o rpc_common.o lookup3.o  ../../../lib/utils/buffer.o ../../../lib/utils/logging.o ../../../lib/utils/memory.o $(THIRD_PARTY_LIB)

API_INCLUDES:=$(wildcard ./*.h ./*.hpp)
$(warning $(DIR_LIB_UTILS))
CINCS:=$(INC_UTILS) $(INC_REACTOR) -I./ -I./server -I./client -I../../../server -I../../../server/util -I../../../server/common -I../../../lib/include $(INC_LOG4CPP) $(INC_PROTOBUF) $(INC_EVENT) $(INC_STAT)

.PHONEY = all clean 

all: $(RPC_SERVER_TEST) $(RPC_CLIENT_TEST)

$(RPC_SERVER_TEST): $(RPC_SERVER_TEST_OBJS)
	$(CXX) -g -o $@  $(RPC_SERVER_TEST_OBJS) $(CINCS) -lrt

$(RPC_CLIENT_TEST): $(RPC_CLIENT_TEST_OBJS)
	$(CXX) -g -o $@ $(RPC_CLIENT_TEST_OBJS) $(CINCS) -lrt

$(STATIC_LIB_EVENT):
	make -C $(XM_SERVER_LIB_SOURCE_PATH) static-libevent

$(STATIC_LIB_REACTOR):
	make -C $(XM_SERVER_LIB_SOURCE_PATH)/lreactor 

$(RPC_LIB): $(RPC_OBJS)
	ar rc $(RPC_LIB) $(RPC_OBJS)

clean:
	$(RM) $(RPC_LIB) $(RPC_SERVER_TEST) $(RPC_CLIENT_TEST) *.o
