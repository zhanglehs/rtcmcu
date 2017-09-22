DIR_INC = .
DIR_SRC = .
DIR_OBJ = .
DIR_LOCAL_LIB = .
DIR_SER = ./server
DIR_CLI = ./client

SRC = $(wildcard *.cpp)
OBJ = $(patsubst %.cpp, %.o, $(notdir $(SRC)))
SRC_SER = $(wildcard $(DIR_SER)/*.cpp)
SRC_CLI = $(wildcard $(DIR_CLI)/*.cpp)
OBJ_CLI = $(patsubst %.cpp, $(DIR_CLI)/%.o, $(notdir $(SRC_CLI)))

UTILS_INCLUDES:=$(wildcard $(DIR_LIB_UTILS)/*.h $(DIR_LIB_UTILS)/*.hpp)

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(patsubst %/,%,$(dir $(mkfile_path)))
source_dir  := $(current_dir)/../../../lib
ifeq	($(strip $(XM_SERVER_LIB_SOURCE_PATH)),)
	XM_SERVER_LIB_SOURCE_PATH = $(source_dir)
endif		

CC = g++
CFLAGS = -g -m64 -Wall -fomit-frame-pointer -pipe -DHAVE_SCHED_GETAFFINITY 
include $(XM_SERVER_LIB_SOURCE_PATH)/makefile.incl
THIRD_PARTY_LIB=$(STATIC_LIB_EVENT) $(STATIC_LIB_LOG4CPP) $(STATIC_LIB_PROTOBUF) $(STATIC_LIB_LOG4CPP) $(STATIC_LIB_REACTOR)

#API_OBJS = cmd_data_opr.o cmd_fsm.o cmd_server.o cmd_fsm_mgr.o ../utils/buffer.o ../lreactor/libreactor.a $(THIRD_PARTY_LIB)

API_LIB=$(DIR_LOCAL_LIB)/libserver.a 
CLIENT = $(DIR_CLI)/test_client

API_INCLUDES:=$(wildcard ./*.h ./*.hpp)
$(warning $(DIR_LIB_UTILS))
CINCS:=$(INC_UTILS) $(INC_REACTOR) -I./ -I./server -I./client -I../../../server -I../../../server/util -I../../../server/common -I../../../lib/include $(INC_LOG4CPP) $(INC_PROTOBUF) $(INC_EVENT) $(INC_STAT)

.PHONEY = all clean 

$(DIR_OBJ)/%.o:$(DIR_SRC)/%.cpp
	$(CC) -g -c $(CFLAGS) $(CINCS) $^ -o $@
$(DIR_SER)/%.o:$(DIR_SER)/%.cpp
	$(CC) -g -c $(CFLAGS) $(CINCS) $^ -o $@
$(DIR_SER)/%.o:$(DIR_SER)/%.cc
	$(CC) -g -c $(CFLAGS) $(CINCS) $^ -o $@
$(DIR_CLI)/%.o:$(DIR_CLI)/%.cpp
	$(CC) -g -c $(CFLAGS) $(CINCS) $^ -o $@

all: $(API_LIB) $(SERVER) $(RPC_CLIENT)
#$(CLIENT)

$(SERVER): $(SERVER_OBJS) ../../util/log.o ../../util/statfile.o
	$(CXX) -g -o $@  $(SERVER_OBJS) $(CINCS) -lrt

$(CLIENT): $(OBJ_CLI) ../../../lib/utils/buffer.o ../../../server/util/log.o ../../../server/util/statfile.o
	g++ -g -o $@ $^ 

$(RPC_CLIENT): $(RPC_CLIENT_OBJS) ../../util/log.o ../../util/statfile.o
	$(CXX) -g -o $@ $^ $(CINCS) -lrt

$(STATIC_LIB_EVENT):
	make -C $(XM_SERVER_LIB_SOURCE_PATH) static-libevent

$(STATIC_LIB_REACTOR):
	make -C $(XM_SERVER_LIB_SOURCE_PATH)/lreactor all
$(API_LIB): $(OBJ)
	ar rc $(API_LIB) $(OBJ)

clean:
	$(RM) $(API_LIB) $(OBJ) $(DIR_SER)/*.o $(DIR_CLI)/*.o $(CLIENT) 
