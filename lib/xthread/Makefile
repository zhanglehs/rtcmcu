mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(patsubst %/,%,$(dir $(mkfile_path)))
source_dir  := $(current_dir)/../..

ifeq	($(strip $(XM_SERVER_LIB_SOURCE_PATH)),)
	XM_SERVER_LIB_SOURCE_PATH = $(source_dir)/lib
endif		

include $(XM_SERVER_LIB_SOURCE_PATH)/makefile.incl
include $(source_dir)/makefile.incl

API_OBJS = condition.o  fast_mutex.o    monitor.o    thread.o 


SOCK_CLIENT_OBJS=sock_client.o

API_LIB= libxthread.a
API_INCLUDES:=$(wildcard ./*.h ./*.hpp)

SOCK_SERVER=sock_server
SOCK_CLIENT=sock_client

.PHONEY = all clean install unstall

all: $(API_LIB)

$(API_LIB): $(API_OBJS)
	$(AR) $@ $^
$(SOCK_CLIENT):$(SOCK_CLIENT_OBJS)
	$(CXX)  $(CXXFLAGS) -o $@  $^ -L.  -lniosocket
clean:
	$(RM) $(API_LIB) $(API_OBJS)
install:
	$(CP) -f $(API_LIB)  /usr/local/lib
	mkdir -p /usr/local/include/xthread/
	$(CP) -f $(API_INCLUDES) /usr/local/include/xthread/
unstall:
	rm -rf /usr/local/lib/libxthread.a
	rm -rf /usr/local/include/xthread/
