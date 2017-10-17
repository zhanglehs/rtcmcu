include env.mk
include ../../lib/makefile.incl

ifndef release
#export OPSCFLAGS = -O0 -fprofile-arcs -ftest-coverage
export OPSCFLAGS = -O0
export OPSLIBS = -lgcov
export OPSDEF = -DDEBUG
else
export OPSCFLAGS = -O2
export OPSLIBS = 
endif

CFLAGS += $(OPSCFLAGS) -std=c++11
LIBS += $(OPSLIBS)
DEFS += $(OPSDEF)

ifdef ptmalloc
export MALLOC_FLAGS =
export PROFILER_FLAGS =
else
export INC_MALLOC = -I /usr/local/include
export MALLOC_FLAGS = -ltcmalloc
export PROFILER_FLAGS = -lprofiler
endif

PRJLIB_DIR = $(SOURCE_DIR)/lib
UTIL_DIR = ../util
COMMON_DIR = ../common

THIRD_PARTY_DIR = $(SOURCE_DIR)third_party

THIRD_PARTY_LIB = $(THIRD_PARTY_DIR)/json-c-0.11/.libs/libjson-c.a \
                  $(THIRD_PARTY_DIR)/libevent-2.1.8-stable/out/lib/libevent.a \
				  $(THIRD_PARTY_DIR)/protobuf-3.2.0/out/lib/libprotobuf.a \
				  $(THIRD_PARTY_DIR)/log4cplus-1.1.2/out/lib/liblog4cplus.a \
				  $(STATIC_LIB_STAT) $(STATIC_LIB_REACTOR)

MEDIA_MANAGER_DIR = media_manager
PUBLISHER_DIR = publisher
BACKEND_DIR = backend_new
PROTOBUF_DIR = ../common/protobuf
FRAGMENT_DIR = fragment
AVCODEC_DIR = avcodec
AVFORMAT_DIR = avformat
NETWORK_DIR = network
PLAYER_DIR = player
UPLOADER_DIR = uploader
TRANSPORT_DIR = transport
RTPTRANS_DIR = rtp_trans
CMDSERVER_DIR = ../network/cmdserver

CINCS += -I$(MEDIA_MANAGER_DIR)/ -I$(FRAGMENT_DIR)/ -I$(UPLOADER_DIR) -I$(BACKEND_DIR) -I$(PUBLISHER_DIR) -I$(COMMON_DIR)
CINCS += -I. 
CINCS += -I$(THIRD_PARTY_DIR)/json-c-0.11 \
         -I$(THIRD_PARTY_DIR)/libevent-2.1.8-stable/out/include \
		 -I$(THIRD_PARTY_DIR)/protobuf-3.2.0/out/include \
		 -I$(THIRD_PARTY_DIR)/log4cplus-1.1.2/out/include \
		 $(INC_STAT) $(INC_UTILS) $(INC_REACTOR) -I../network -I$(CMDSERVER_DIR)
CINCS += -I$(UTIL_DIR)
CINCS += $(INC_MALLOC)
FORWARD = forward_rtp
RECEIVER = receiver_rtp

BIN = $(FORWARD) $(RECEIVER)


PUBLISHER_OBJS = $(PUBLISHER_DIR)/flv_publisher.o
MEDIA_MANAGER_OBJS = $(MEDIA_MANAGER_DIR)/cache_manager.o $(MEDIA_MANAGER_DIR)/cache_manager_config.o $(MEDIA_MANAGER_DIR)/cache_manager_state.o \
					 $(MEDIA_MANAGER_DIR)/stream_store.o $(MEDIA_MANAGER_DIR)/rtp_block_cache.o\
					 $(MEDIA_MANAGER_DIR)/circular_cache.o  \
					 $(MEDIA_MANAGER_DIR)/jitter_buffer.o $(MEDIA_MANAGER_DIR)/rtp2flv_remuxer.o \
					 $(MEDIA_MANAGER_DIR)/flv_miniblock_circular_cache.o
FRAGMENT_OBJS = $(FRAGMENT_DIR)/flv_block.o $(FRAGMENT_DIR)/fragment_generator.o \
				$(FRAGMENT_DIR)/rtp_block.o $(FRAGMENT_DIR)/flv_miniblock.o

AVFORMAT_OBJS = $(AVFORMAT_DIR)/rtcp_dec.o $(AVFORMAT_DIR)/rtcp_enc.o $(AVFORMAT_DIR)/rtcp_helper.o $(AVFORMAT_DIR)/rtp_enc.o \
	$(AVFORMAT_DIR)/rtp_enc_aac.o $(AVFORMAT_DIR)/rtp_enc_h264.o $(AVFORMAT_DIR)/sdp.o $(AVFORMAT_DIR)/AMF.o $(AVFORMAT_DIR)/FLV.o

AVCODEC_OBJS = $(AVCODEC_DIR)/aac.o $(AVCODEC_DIR)/h264.o

TRANSPORTOBJS = $(TRANSPORT_DIR)/rtp_send_queue.o

RTP_TRANS_OBJS = $(RTPTRANS_DIR)/rtp_session.o $(RTPTRANS_DIR)/rtp_trans.o $(RTPTRANS_DIR)/rtp_trans_manager.o \
                 $(RTPTRANS_DIR)/rtp_sender_session.o $(RTPTRANS_DIR)/rtp_receiver_session.o $(RTPTRANS_DIR)/rtp_sender_trans.o $(RTPTRANS_DIR)/rtp_receiver_trans.o

COMMONOBJS = hls.o media_buffer.o perf.o info_collector.o\
			 whitelist_manager.o libcachemanager.a libfragment.a portal.o config_manager.o \
			 $(UTIL_DIR)/connection.o\
			 $(NETWORK_DIR)/base_http_server.o\
			 $(COMMON_DIR)/proto.o $(COMMON_DIR)/proto_rtp_rtcp.o \
			 $(UTIL_DIR)/log.o $(DIR_LIB_UTILS)/buffer.o $(DIR_LIB_UTILS)/logging.o $(UTIL_DIR)/xml.o \
			 $(UTIL_DIR)/tsmux.o $(UTIL_DIR)/flv2ts.o $(DIR_LIB_UTILS)/memory.o $(UTIL_DIR)/session.o \
			 $(UTIL_DIR)/md5.o $(UTIL_DIR)/access.o $(UTIL_DIR)/hashtable.o $(UTIL_DIR)/rbtree.o \
			 $(UTIL_DIR)/util.o $(UTIL_DIR)/backtrace.o $(UTIL_DIR)/levent.o $(UTIL_DIR)/city.o $(UTIL_DIR)/bits.o\
			 $(UTIL_DIR)/statfile.o $(UTIL_DIR)/report.o $(UTIL_DIR)/report_stats.o $(UTIL_DIR)/flv.o $(UTIL_DIR)/murmur_hash.o\
			 $(AVFORMAT_OBJS) $(AVCODEC_OBJS) $(PUBLISHER_OBJS) \
			 $(LIBXML) $(LIBUUID) $(THIRD_PARTY_LIB) $(PROTOBUF_DIR)/proto_rtp_ext.pb.o

PLAYEROBJS = $(PLAYER_DIR)/module_player.o \
			 $(PLAYER_DIR)/rtp_player_config.o $(PLAYER_DIR)/flv_live_player.o

TRACKEROBJS = module_tracker.o
ifeq ($(BACKEND_DIR), backend_new)
BACKENDOBJS = $(PROTOBUF_DIR)/InterliveServerStat.pb.o $(PROTOBUF_DIR)/f2t_register_req.pb.o $(BACKEND_DIR)/module_backend.o \
				$(BACKEND_DIR)/forward_common.o $(BACKEND_DIR)/forward_server.o \
				$(BACKEND_DIR)/rtp_backend_config.o \
				$(BACKEND_DIR)/forward_client_rtp_tcp.o
else
BACKENDOBJS = $(PROTOBUF_DIR)/InterliveServerStat.pb.o $(PROTOBUF_DIR)/f2t_register_req.pb.o $(BACKEND_DIR)/module_backend.o \
				$(BACKEND_DIR)/forward_common.o $(BACKEND_DIR)/forward_server.o $(BACKEND_DIR)/forward_client.o \
				$(BACKEND_DIR)/forward_server_v2.o $(BACKEND_DIR)/forward_client_v2.o \
				$(BACKEND_DIR)/rtp_backend_config.o
endif

UPLOADEROBJS = $(UPLOADER_DIR)/RtpTcpConnectionManager.o $(UPLOADER_DIR)/uploader_config.o $(UPLOADER_DIR)/rtp_uploader_config.o

FORWARDOBJS = forward_target.o $(UPLOADEROBJS) $(PLAYEROBJS) $(TRACKEROBJS) $(BACKENDOBJS) $(RTP_TRANS_OBJS) $(COMMONOBJS) $(TRANSPORTOBJS)
FORWARDLIBS = $(LIBS) -luuid -lexpat -lpthread

RECEIVEROBJS = receiver.o $(UPLOADEROBJS) $(PLAYEROBJS) $(TRACKEROBJS) $(BACKENDOBJS) $(RTP_TRANS_OBJS) $(COMMONOBJS) $(TRANSPORTOBJS)
RECEIVERLIBS = $(LIBS) -luuid -lexpat -lpthread

#all:add_hook version_file $(FORWARD) $(RECEIVER)
all: version_file $(FORWARD) $(RECEIVER)
	@echo -e "\033[32m[INF] Make interlive successfully. \033[0m"
	@echo

add_hook:
	@rm ../../.git/hooks/pre-commit -f
	@ln -s ../../build/hooks/pre-commit ../../.git/hooks/pre-commit

lcov:
	lcov --directory ../ --capture --output-file interlive.info
	genhtml -o fresults interlive.info

version_file:
	@if [ ! -f ../../lib/version.h ]; then \
		cd ../..; ./generate_version.sh; \
	fi;

forward_target.o:target.cpp 
	make version_file
	$(CC) $(CFLAGS) $(CINCS) -c -DMODULE_TRACKER -DMODULE_BACKEND -DMODULE_UPLOADER -DMODULE_PLAYER -DPROCESS_NAME='"forward_rtp"' $(DEFS) $^ -o $@
receiver.o:target.cpp 
	make version_file
	$(CC) $(CFLAGS) $(CINCS) -c -DMODULE_TRACKER -DMODULE_BACKEND -DMODULE_UPLOADER -DMODULE_PLAYER -DPROCESS_NAME='"receiver_rtp"' $(DEFS) $^ -o $@

$(FORWARD):$(THIRD_PARTY_LIB) util_o common_o $(FORWARDOBJS)
	$(CC) $(CFLAGS) $(FORWARDOBJS) $(FORWARDLIBS) $(LFLAGS) $(MALLOC_FLAGS) $(PROFILER_FLAGS) -o $@
	@echo -e "\033[32m[INF] Make forward successfully. \033[0m"
	@echo
$(RECEIVER):$(THIRD_PARTY_LIB) util_o common_o $(RECEIVEROBJS)
	$(CC) $(CFLAGS) $(RECEIVEROBJS) $(RECEIVERLIBS) $(LFLAGS) $(MALLOC_FLAGS) $(PROFILER_FLAGS) -o $@
	@echo -e "\033[32m[INF] Make receiver successfully. \033[0m"
	@echo

util_o:
	@cd $(UTIL_DIR);make all;

common_o:
	@cd $(COMMON_DIR);make all;

$(STATIC_LIB_STAT):
	make -C $(DIR_LIB) static-libstat

$(STATIC_LIB_REACTOR):
	make -C $(DIR_LIB_REACTOR) all

clean-lib:
	@cd $(DIR_LIB); make clean-libevent; make clean-json-c; make clean-libstat; make clean-lreactor;
	@cd $(DIR_LIB); rm -f version.h;
	@cd $(CMDSERVER_DIR); make clean;
	@echo
	@echo -e "\033[32m[INF] Clean lib successfully. \033[0m"
	@echo

clean:
	@cd $(UTIL_DIR);make clean
	@cd $(COMMON_DIR);make clean
	@cd $(MEDIA_MANAGER_DIR); rm -f *.o *.a
	@cd $(AVFORMAT_DIR); rm -f *.o *.a
	@cd $(AVCODEC_DIR); rm -f *.o *.a
	@cd $(FRAGMENT_DIR); rm -f *.o *.a
	@cd $(UPLOADER_DIR); rm -f *.o *.a
	@cd $(PLAYER_DIR); rm -f *.o
	@cd $(PROTOBUF_DIR); rm -f *.o
	@cd $(RTPTRANS_DIR); rm -f *.o
	@cd $(BACKEND_DIR); rm -f *.o
	@cd $(NETWORK_DIR); rm -f *.o
	@cd $(TRANSPORT_DIR); rm -f *.o 
	@cd $(PUBLISHER_DIR); rm -f *.o 

	@echo -e "\033[32m[INF] Clean interlive successfully. \033[0m"
	@echo

cleanxmode:
	@find . -regex ".*\.cpp\|.*\.h\|.*\.c" -perm /a+x | xargs chmod -x
	
	@echo -e "\033[32m[INF] Clean x mode successfully. \033[0m"

cleangcov:
	rm -f *.gcda *.gcno
	@cd $(UTIL_DIR);make cleangcov
	@cd $(COMMON_DIR);make cleangcov

cleanall:clean-lib clean cleangcov

libcachemanager.a: $(MEDIA_MANAGER_DIR)/cache_manager.h $(MEDIA_MANAGER_DIR)/circular_cache.h $(MEDIA_MANAGER_DIR)/cache_manager_config.h $(MEDIA_MANAGER_OBJS)
	ar rcs libcachemanager.a $(MEDIA_MANAGER_OBJS)

libfragment.a: $(FRAGMENT_DIR)/fragment.h $(FRAGMENT_DIR)/fragment_generator.h $(FRAGMENT_OBJS)
	ar rcs libfragment.a $(FRAGMENT_OBJS)
TARGETTYPE := APP
TARGETNAME := 
