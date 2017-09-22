MAKE= make

DIR_LIB 	= lib
DIR_XMANTS 	= xm_ants  
DIR_INTERLIVE = server/interlive
DIR_DEPLOY = build/deploy

DIRS = $(DIR_LIB) $(DIR_XMANTS)


mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(patsubst %/,%,$(dir $(mkfile_path)))

export XM_SERVER_LIB_SOURCE_PATH := $(current_dir)/lib

ifdef release
	include ./build/release.mk
	build="release"
else
	include ./build/debug.mk
	build="debug"
endif

ifndef DESTDIR
	DESTDIR=/opt/interlive
endif

ifdef prefix
	DESTDIR=$(prefix)
endif

ifndef test
	test="false"
endif

#default: clear-version all

receiver:
	@if [ ${build} == "release" ]; then \
		  make clean; make gen-version release=${release} ; \
	else \
		  make gen-version; \
	fi
#	$(MAKE) -C $(DIR_LIB)
	$(MAKE) -C $(DIR_INTERLIVE) receiver

receiver_rtp:
	@if [ ${build} == "release" ]; then \
		  make clean; make gen-version release=${release} ; \
	else \
		  make gen-version; \
	fi
#	$(MAKE) -C $(DIR_LIB)
	$(MAKE) -C $(DIR_INTERLIVE)_rtp receiver_rtp

forward-fp:
	@if [ ${build} == "release" ]; then \
		  make clean; make gen-version release=${release} ; \
	else \
		  make gen-version; \
	fi 
#	$(MAKE) -C $(DIR_LIB)
	$(MAKE) -C $(DIR_INTERLIVE) forward
	@cd $(DIR_INTERLIVE); \
	cp forward-fp.xml forward.xml -f;

forward-fp_rtp:
	@if [ ${build} == "release" ]; then \
		  make clean; make gen-version release=${release} ; \
	else \
		  make gen-version; \
	fi 
#	$(MAKE) -C $(DIR_LIB)
	$(MAKE) -C $(DIR_INTERLIVE)_rtp forward_rtp
	@cd $(DIR_INTERLIVE)_rtp; \
	cp forward-fp_rtp.xml forward_rtp.xml -f;

forward-nfp:
	@if [ ${build} == "release" ]; then \
		  make clean; make gen-version release=${release} ; \
	else \
		  make gen-version; \
	fi 
#	$(MAKE) -C $(DIR_LIB)
	$(MAKE) -C $(DIR_INTERLIVE) forward
	@cd $(DIR_INTERLIVE); \
	cp forward-nfp.xml forward.xml -f;

forward-nfp_rtp:
	@if [ ${build} == "release" ]; then \
		  make clean; make gen-version release=${release} ; \
	else \
		  make gen-version; \
	fi 
#	$(MAKE) -C $(DIR_LIB)
	$(MAKE) -C $(DIR_INTERLIVE)_rtp forward_rtp
	@cd $(DIR_INTERLIVE)_rtp; \
	cp forward-nfp_rtp.xml forward_rtp.xml -f;

interlive:
#	$(MAKE) -C $(DIR_LIB)
	$(MAKE) -C $(DIR_INTERLIVE)

all: prjlib interlive ants	
	@echo -e "\033[32m[INF] Make all successfully. \033[0m"
	@echo

clear-version:
	@./generate_version.sh
	@echo -e "\033[32m[INF] UnVersion live_stream_svr successfully. \033[0m"
	@echo

cleanall: 
	$(MAKE) -C $(DIR_LIB) cleanall
	$(MAKE) -C $(DIR_XMANTS) clean
	$(MAKE) -C $(DIR_INTERLIVE) cleanall
	$(MAKE) -C $(DIR_DEPLOY) clean
	@echo -e "\033[32m[INF] Clean live_stream_svr successfully. \033[0m"
	@echo

clean: 
	$(MAKE) -C $(DIR_LIB) clean
	$(MAKE) -C $(DIR_XMANTS) clean
	$(MAKE) -C $(DIR_INTERLIVE) clean
	$(MAKE) -C $(DIR_DEPLOY) clean
	@echo -e "\033[32m[INF] Clean live_stream_svr successfully. \033[0m"
	@echo

gen-version:
	@./generate_version.sh ${build} ${release}
	@cat lib/version.h
	@echo -e "\033[32m[INF] Version live_stream_svr successfully. \033[0m"
	@echo

prjlib:
	$(MAKE) -C $(DIR_LIB)

ants:
	@if [ ${build} == "release" ]; then \
		  make clean; make gen-version release=${release} ; \
	else \
		  make gen-version; \
	fi 
	$(MAKE) -C $(DIR_XMANTS)

xm_ants-release: 
	@make clean 
	@make gen-version release=1 
	@make ants
	@echo -e "\033[32m[INF] Release live_stream_svr successfully. \033[0m"
	@echo

install-libs:
	mkdir -p $(DESTDIR)/lib64
	cp -d /usr/local/lib/libunwind.so* $(DESTDIR)/lib64
	cp -d /usr/local/lib/libtcmalloc.so* $(DESTDIR)/lib64
	cp -d /usr/local/lib/libprofiler.so* $(DESTDIR)/lib64

install-xm_ants: install-libs
	mkdir -p $(DESTDIR)/xm_ants/
	cp -f xm_ants/xm_ants xm_ants/xm_ants.conf $(DESTDIR)/xm_ants/
	cp -f xm_ants/xm_ants xm_ants/xm_ants.sh $(DESTDIR)/xm_ants/
	@if [ ${test} != "true" ]; then\
		sed -i 's/log4cplus.test.properties/log4cplus.properties/g' $(DESTDIR)/xm_ants/xm_ants.conf; \
	fi
	cp -rf xm_ants/scripts/ $(DESTDIR)/xm_ants/scripts/
	cp -rf xm_ants/config/ $(DESTDIR)/xm_ants/config/
	cp -rf bin/ $(DESTDIR)/xm_ants/bin/

install-receiver: install-libs
	mkdir -p $(DESTDIR)/receiver/scripts
	cp -f server/interlive/receiver server/interlive/receiver.xml server/interlive/crossdomain.xml server/interlive/log4cplus.cfg $(DESTDIR)/receiver
	cp -f server/interlive/scripts/* $(DESTDIR)/receiver/scripts

install-receiver_rtp: install-libs
	mkdir -p $(DESTDIR)/receiver_rtp/scripts
	cp -f server/interlive_rtp/receiver_rtp server/interlive_rtp/receiver_rtp.xml server/interlive_rtp/crossdomain.xml server/interlive_rtp/log4cplus.cfg $(DESTDIR)/receiver_rtp
	cp -f server/interlive_rtp/scripts/* $(DESTDIR)/receiver_rtp/scripts

install-forward-fp: install-libs
	mkdir -p $(DESTDIR)/forward/scripts
	cp -f server/interlive/forward server/interlive/forward.xml server/interlive/crossdomain.xml server/interlive/log4cplus.cfg $(DESTDIR)/forward
	cp -f server/interlive/scripts/* $(DESTDIR)/forward/scripts

install-forward-fp_rtp: install-libs
	mkdir -p $(DESTDIR)/forward_rtp/scripts
	cp -f server/interlive_rtp/forward_rtp server/interlive_rtp/forward_rtp.xml server/interlive_rtp/crossdomain.xml server/interlive_rtp/log4cplus.cfg $(DESTDIR)/forward_rtp
	cp -f server/interlive_rtp/scripts/* $(DESTDIR)/forward_rtp/scripts

install-forward-nfp: install-libs
	mkdir -p $(DESTDIR)/forward/scripts
	cp -f server/interlive/forward server/interlive/forward.xml server/interlive/crossdomain.xml server/interlive/log4cplus.cfg $(DESTDIR)/forward
	cp -f server/interlive/scripts/* $(DESTDIR)/forward/scripts

install-forward-nfp_rtp: install-libs
	mkdir -p $(DESTDIR)/forward_rtp/scripts
	cp -f server/interlive_rtp/forward_rtp server/interlive_rtp/forward_rtp.xml server/interlive_rtp/crossdomain.xml server/interlive_rtp/log4cplus.cfg $(DESTDIR)/forward_rtp
	cp -f server/interlive_rtp/scripts/* $(DESTDIR)/forward_rtp/scripts
