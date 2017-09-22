mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(patsubst %/,%,$(dir $(mkfile_path)))
source_dir  := $(current_dir)/../..

ifeq	($(strip $(XM_SERVER_LIB_SOURCE_PATH)),)
	XM_SERVER_LIB_SOURCE_PATH = $(source_dir)/lib
endif		

include $(XM_SERVER_LIB_SOURCE_PATH)/makefile.incl

API_OBJS =  bits.o byteorder.o selectionkey.o   selectablechannel.o \
	 serversocketchannel.o socketchannel.o  epollselector.o pollselector.o \
	 net.o selectorprovider.o reactor.o active_reactor.o connectioninfo.o  

SOCK_SERVER_OBJS=sock_server.o
SOCK_CLIENT_OBJS=sock_client.o

API_LIB=$(DIR_LIBRARY)/libxsocket.a
API_INCLUDES:=$(wildcard ./*.h ./*.hpp)

SOCK_SERVER=sock_server
SOCK_CLIENT=sock_client

INC_LNK_SPARSEHASH=-I$(DIR_LIB_SPARSEHASH)/include -L$(DIR_LIB_SPARSEHASH)/lib/pkgconfig
CXXFLAGS:=$(CXXFLAGS) $(INC_LNK_SPARSEHASH)

.PHONEY = all clean install unstall

all: $(API_LIB) #$(SOCK_SERVER) $(SOCK_CLIENT)

$(API_LIB): $(API_OBJS)
	$(AR) $@ $^
$(SOCK_SERVER):$(SOCK_SERVER_OBJS)
	$(CXX)  $(CXXFLAGS) -o $@  $^ -L.  -lxsocket $(INC_LNK_SPARSEHASH)
$(SOCK_CLIENT):$(SOCK_CLIENT_OBJS)
	$(CXX)  $(CXXFLAGS) -o $@  $^ -L.  -lxsocket $(INC_LNK_SPARSEHASH)
clean:
	$(RM) $(API_LIB) $(API_OBJS) $(SOCK_SERVER) $(SOCK_SERVER_OBJS) $(SOCK_CLIENT) $(SOCK_CLIENT_OBJS)	
install:
	$(CP) -f $(API_LIB)  /usr/local/lib
	mkdir -p /usr/local/include/xsocket/
	$(CP) -f $(API_INCLUDES) /usr/local/include/xsocket/
unstall:
	rm -rf /usr/local/lib/libxsocket.a
	rm -rf /usr/local/include/xsocket/
