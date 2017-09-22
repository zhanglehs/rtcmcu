mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(patsubst %/,%,$(dir $(mkfile_path)))
SOURCE_DIR:= $(current_dir)/..

include ./makefile.incl
include ../makefile.incl

.PHONEY : all clean install unstall

all:static-json-c static-libevent static-librtmp static-libstat
	$(MAKE) -C $(DIR_LIB_NET)
	$(MAKE) -C $(DIR_LIB_UTILS)
	$(MAKE) -C $(DIR_LIB_JSONCPP)
	$(MAKE) -C $(DIR_LIB_APPFRAME)
	$(MAKE) -C $(DIR_LIB_XTHREAD)
	#$(MAKE) -C $(DIR_LIB_XSOCKET)
	rm */*.so* -f
	@echo
	@echo -e "\033[32m[INF] Make lib successfully. \033[0m"
	@echo

static-json-c: 
	@cd $(DIR_LIB_JSONC); \
	if [ ! -x "configure" ]; then \
		chmod u+x configure; \
	fi; \
	if [ ! -f "config.status" ]; then \
		./configure; \
	else \
    	echo "json-c: config.status file exist, skip configure."; \
	fi; \
	make
	@echo
	@echo -e "\033[32m[INF] Make json-c successfully. \033[0m"
	@echo

clean-json-c:
	@cd $(DIR_LIB_JSONC); \
	if [ -f "Makefile" ]; then \
		make clean; \
		echo; \
		echo -e "\033[32m[INF] Clean json-c successfully. \033[0m" ;\
		echo;\
	else \
	    echo -e "\033[33m[WRN] Can not found Makefile in json-c, skip clean json. \033[0m"; \
	fi

static-libevent: 
	@cd $(DIR_LIB_EVENT); \
	if [ ! -x "configure" ]; then \
		chmod u+x configure; \
	fi; \
	if [ ! -f "config.status" ]; then \
		./configure --prefix=$(DIR_LIBRARY)/; \
	else \
    	echo "libevent: config.status file exist, skip configure."; \
	fi; \
	make

clean-libevent:
	@cd $(DIR_LIB_EVENT); \
	if [ -f "Makefile" ]; then \
		make clean; \
		rm -f Makefile config.h config.status event-config.h libtool sample/Makefile stamp-h1 test/Makefile version.h config.h.in~; \
		rm -rf autom4te.cache; \
		echo; \
		echo -e "\033[32m[INF] Clean libevent successfully. \033[0m" ;\
		echo;\
	else \
	    echo -e "\033[33m[WRN] Can not found Makefile in libevent, skip clean libevent. \033[0m"; \
	fi

static-librtmp: 
	@cd $(DIR_LIB_RTMP); \
	make; 
	
clean-librtmp:
	@cd $(DIR_LIB_RTMP); \
	if [ -f "Makefile" ]; then \
		make clean; \
		echo; \
		echo -e "\033[32m[INF] Clean librtmp successfully. \033[0m" ;\
		echo;\
	else \
	    echo -e "\033[33m[WRN] Can not found Makefile in librtmp, skip clean librtmp. \033[0m"; \
	fi

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

clean-lreactor:
	@cd $(DIR_LIB_REACTOR); \
	make clean;

clean:clean-json-c clean-libevent clean-librtmp clean-libstat clean-lreactor
#	$(MAKE) -C $(DIR_LIB_XSOCKET)   clean
	$(MAKE) -C $(DIR_LIB_XTHREAD)   clean
	$(MAKE) -C $(DIR_LIB_APPFRAME)  clean
	$(MAKE) -C $(DIR_LIB_NET)  		clean
	$(MAKE) -C $(DIR_LIB_UTILS)  	clean
	$(MAKE) -C $(DIR_LIB_JSONCPP)		clean

	rm -f *.a *.la
	rm version.h

cleanall: clean

#install:
#	$(MAKE) -C $(DIR_LIB_EVENT)   	install
#	$(MAKE) -C $(DIR_LIB_XSOCKET)   install
#	$(MAKE) -C $(DIR_LIB_XTHREAD)   install
#	$(MAKE) -C $(DIR_LIB_APPFRAME)  install
#	$(MAKE) -C $(DIR_LIB_NET)  		install
#	$(MAKE) -C $(DIR_LIB_UTILS)  	install
#	$(MAKE) -C $(DIR_LIB_JSON)		install
#	$(MAKE) -C $(DIR_LIB_RTMP)		install

#unstall:
#	$(MAKE) -C $(DIR_LIB_EVENT)   	uninstall
#	$(MAKE) -C $(DIR_LIB_XSOCKET)   uninstall
#	$(MAKE) -C $(DIR_LIB_XTHREAD)   uninstall
#	$(MAKE) -C $(DIR_LIB_APPFRAME)  uninstall
#	$(MAKE) -C $(DIR_LIB_NET)  		uninstall
#	$(MAKE) -C $(DIR_LIB_UTILS)  	uninstall
#	$(MAKE) -C $(DIR_LIB_JSON) 		uninstall
#	$(MAKE) -C $(DIR_LIB_RTMP) 		uninstall

