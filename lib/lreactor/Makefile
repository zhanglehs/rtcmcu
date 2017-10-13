mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(patsubst %/,%,$(dir $(mkfile_path)))
source_dir  := $(current_dir)/../..

ifeq	($(strip $(XM_SERVER_LIB_SOURCE_PATH)),)
	XM_SERVER_LIB_SOURCE_PATH = $(source_dir)/lib
endif		

include $(XM_SERVER_LIB_SOURCE_PATH)/makefile.incl

THIRD_PARTY_DIR = $(source_dir)/third_party

THIRD_PARTY_LIB=$(THIRD_PARTY_DIR)/libevent-1.4.14b-stable/out/lib/libevent.a \
				$(THIRD_PARTY_DIR)/protobuf-3.2.0/out/lib/libprotobuf.a \
				$(STATIC_LIB_LOG4CPP)
API_OBJS = event_loop.o channel.o  ip_endpoint.o socket.o sockets_ops.o \
		   tcp_server.o tcp_client.o tcp_connection.o 

API_LIB=libreactor.a
API_INCLUDES:=$(wildcard ./*.h ./*.hpp)
UTILS_INCLUDES:=$(wildcard $(DIR_LIB_UTILS)/*.h $(DIR_LIB_UTILS)/*.hpp)
CINCS = -I$(API_INCLUDES) -I$(UTILS_INCLUDES) -I$(INC_UTILS)
CXXFLAGS+=-I../include $(INC_UTILS) -I$(THIRD_PARTY_DIR)/libevent-1.4.14b-stable/out/include -I$(THIRD_PARTY_DIR)/log4cplus-1.1.2/out/include

SOCK_SERVER = sample_server
SOCK_SERVER_OBJS = sample_server.o ../utils/buffer.o ../utils/memory.o ../utils/logging.o $(API_OBJS) $(THIRD_PARTY_LIB)
SOCK_CLIENT = sample_client
SOCK_CLIENT_OBJS = sample_client.o ../utils/buffer.o ../utils/memory.o ../utils/logging.o $(API_OBJS) $(THIRD_PARTY_LIB)

.PHONEY = all clean 

all: $(API_LIB)
#all: $(API_LIB) $(SOCK_SERVER) $(SOCK_CLIENT) 

#$(STATIC_LIB_EVENT):
#	make -C $(XM_SERVER_LIB_SOURCE_PATH) static-libevent

$(API_LIB): $(API_OBJS)
	ar rcs $(API_LIB) $(API_OBJS)

$(SOCK_SERVER) : $(SOCK_SERVER_OBJS)
	$(CXX) -g -o $@ $(SOCK_SERVER_OBJS) $(INC_EVENT) -L. -lrt -lpthread

$(SOCK_CLIENT) : $(SOCK_CLIENT_OBJS)
	$(CXX) -g -o $@ $(SOCK_CLIENT_OBJS) $(INC_EVENT) -L. -lrt -lpthread

clean:
	$(RM) $(API_LIB) $(SOCK_SERVER) $(SOCK_CLIENT) *.o 
