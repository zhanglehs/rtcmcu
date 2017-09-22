mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(patsubst %/,%,$(dir $(mkfile_path)))
source_dir  := $(current_dir)/../..

ifeq	($(strip $(XM_SERVER_LIB_SOURCE_PATH)),)
	XM_SERVER_LIB_SOURCE_PATH = $(source_dir)/lib
endif		

include $(XM_SERVER_LIB_SOURCE_PATH)/makefile.incl
include $(source_dir)/makefile.incl

API_OBJS =  net_utils.o uri.o
API_LIB= libnet.a

API_INCLUDES:=$(wildcard ./*.h ./*.hpp)

.PHONEY = all clean install unstall

all: $(API_LIB)


$(API_LIB): $(API_OBJS) 
	$(AR) $@ $^

clean:
	$(RM) $(API_LIB) $(API_OBJS)	
