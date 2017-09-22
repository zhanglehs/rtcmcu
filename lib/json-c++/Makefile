mkfile_path  := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir_2:= $(patsubst %/,%,$(dir $(mkfile_path)))
source_dir   := $(current_dir_2)/../..

ifeq	($(strip $(XM_SERVER_LIB_SOURCE_PATH)),)
	XM_SERVER_LIB_SOURCE_PATH = $(source_dir)/lib
endif		

include $(XM_SERVER_LIB_SOURCE_PATH)/makefile.incl
include $(source_dir)/makefile.incl

INCLUDES = -I$(XM_SERVER_LIB_SOURCE_PATH) 

.PHONEY = all clean install unstall

JSON_OBJ= json_reader.o json_value.o json_writer.o
#LIB_JSON= $(DIR_LIBRARY)/libjson.a
LIB_JSON= libjsoncpp.a

all: $(LIB_JSON)

$(LIB_JSON): $(JSON_OBJ)
	$(AR) $@ $^

clean:
	rm -f $(LIB_JSON) $(JSON_OBJ)
