mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(patsubst %/,%,$(dir $(mkfile_path)))
SOURCE_DIR:= $(current_dir)/..

include ./makefile.incl
include ../makefile.incl

.PHONEY : all clean

all:static-libstat
	rm */*.so* -f
	@echo
	@echo -e "\033[32m[INF] Make lib successfully. \033[0m"
	@echo


static-libstat: 
	@cd $(DIR_LIB_STAT); \
	if [ -f "Makefile" ]; then \
		make; \
	else \
	    echo -e "\033[33m[WRN] Can not found Makefile in libstat. \033[0m"; \
	fi

clean-libstat:
	@cd $(DIR_LIB_STAT); \
	make clean;

clean: clean-libstat
	rm -f *.a *.la
	rm version.h
