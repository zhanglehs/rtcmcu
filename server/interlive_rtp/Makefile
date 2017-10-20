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

UTIL_DIR = ../util

THIRD_PARTY_DIR = $(SOURCE_DIR)third_party

THIRD_PARTY_LIB = $(THIRD_PARTY_DIR)/json-c-0.11/.libs/libjson-c.a \
                  $(THIRD_PARTY_DIR)/libevent-2.1.8-stable/out/lib/libevent.a \
				  $(THIRD_PARTY_DIR)/protobuf-3.2.0/out/lib/libprotobuf.a \
				  $(THIRD_PARTY_DIR)/log4cplus-1.1.2/out/lib/liblog4cplus.a \
				  $(STATIC_LIB_STAT) $(STATIC_LIB_REACTOR)

MEDIA_MANAGER_DIR = media_manager
PUBLISHER_DIR = publisher
RELAY_DIR = relay
FRAGMENT_DIR = fragment
AVCODEC_DIR = avcodec
AVFORMAT_DIR = avformat
NETWORK_DIR = network
CONNECTION_MANAGER_DIR = connection_manager
RTPTRANS_DIR = rtp_trans
CMD_PROTOCOL_DIR = cmd_protocol
COMMON_DIR = common

CINCS += -I. \
         -I$(THIRD_PARTY_DIR)/json-c-0.11 \
         -I$(THIRD_PARTY_DIR)/libevent-2.1.8-stable/out/include \
		 -I$(THIRD_PARTY_DIR)/protobuf-3.2.0/out/include \
		 -I$(THIRD_PARTY_DIR)/log4cplus-1.1.2/out/include \
		 $(INC_STAT) $(INC_REACTOR)

RECEIVER = receiver_rtp

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

RTP_TRANS_OBJS = $(RTPTRANS_DIR)/rtp_session.o $(RTPTRANS_DIR)/rtp_trans.o $(RTPTRANS_DIR)/rtp_trans_manager.o \
                 $(RTPTRANS_DIR)/rtp_sender_session.o $(RTPTRANS_DIR)/rtp_receiver_session.o $(RTPTRANS_DIR)/rtp_sender_trans.o $(RTPTRANS_DIR)/rtp_receiver_trans.o

COMMON_OBJS = hls.o perf.o info_collector.o\
			 libcachemanager.a libfragment.a config_manager.o \
			 $(NETWORK_DIR)/base_http_server.o $(NETWORK_DIR)/rtp_send_queue.o \
			 $(UTIL_DIR)/log.o $(DIR_LIB_UTILS)/buffer.o $(DIR_LIB_UTILS)/logging.o $(UTIL_DIR)/xml.o \
			 $(UTIL_DIR)/tsmux.o $(UTIL_DIR)/flv2ts.o $(DIR_LIB_UTILS)/memory.o \
			 $(UTIL_DIR)/md5.o $(UTIL_DIR)/access.o \
			 $(UTIL_DIR)/util.o $(UTIL_DIR)/backtrace.o $(UTIL_DIR)/bits.o\
			 $(UTIL_DIR)/statfile.o $(UTIL_DIR)/flv.o \
			 $(AVFORMAT_OBJS) $(AVCODEC_OBJS) \
			 $(LIBXML) $(LIBUUID) $(THIRD_PARTY_LIB)

CONNECTION_MANAGER_OBJS = $(CONNECTION_MANAGER_DIR)/FlvConnectionManager.o \
			 $(CONNECTION_MANAGER_DIR)/rtp_player_config.o $(CONNECTION_MANAGER_DIR)/flv_live_player.o \
			 $(CONNECTION_MANAGER_DIR)/RtpConnectionManager.o $(CONNECTION_MANAGER_DIR)/RtpConnection.o \
			 $(CONNECTION_MANAGER_DIR)/rtp_uploader_config.o

RELAY_OBJS = $(RELAY_DIR)/rtp_backend_config.o \
			$(RELAY_DIR)/RtpRelay.o $(RELAY_DIR)/RelayManager.o $(RELAY_DIR)/flv_publisher.o

CMD_PROTOCOL_OBJS = $(CMD_PROTOCOL_DIR)/proto.o

COMMON_OBJS += $(COMMON_DIR)/TaskManager.o

RECEIVER_OBJS = receiver.o $(CONNECTION_MANAGER_OBJS) $(RELAY_OBJS) $(RTP_TRANS_OBJS) $(COMMON_OBJS) $(CMD_PROTOCOL_OBJS)
RECEIVER_LIBS = $(LIBS) -luuid -lexpat -lpthread

#all:add_hook version_file $(FORWARD) $(RECEIVER)
all: version_file $(RECEIVER)
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

receiver.o:target.cpp 
	make version_file
	$(CC) $(CFLAGS) $(CINCS) -c -DPROCESS_NAME='"receiver_rtp"' $(DEFS) $^ -o $@

$(RECEIVER):$(THIRD_PARTY_LIB) util_o $(RECEIVER_OBJS)
	$(CC) $(CFLAGS) $(RECEIVER_OBJS) $(RECEIVER_LIBS) $(LFLAGS) $(MALLOC_FLAGS) $(PROFILER_FLAGS) -o $@
	@echo -e "\033[32m[INF] Make receiver successfully. \033[0m"
	@echo

util_o:
	@cd $(UTIL_DIR);make all;

$(STATIC_LIB_STAT):
	make -C $(DIR_LIB) static-libstat

$(STATIC_LIB_REACTOR):
	make -C $(DIR_LIB_REACTOR) all

clean-lib:
	@cd $(DIR_LIB); make clean-libevent; make clean-json-c; make clean-libstat; make clean-lreactor;
	@cd $(DIR_LIB); rm -f version.h;
	@echo
	@echo -e "\033[32m[INF] Clean lib successfully. \033[0m"
	@echo

clean:
	@rm -f *.o *.a
	@cd $(UTIL_DIR);make clean
	@cd $(MEDIA_MANAGER_DIR); rm -f *.o *.a
	@cd $(AVFORMAT_DIR); rm -f *.o *.a
	@cd $(AVCODEC_DIR); rm -f *.o *.a
	@cd $(FRAGMENT_DIR); rm -f *.o *.a
	@cd $(CONNECTION_MANAGER_DIR); rm -f *.o
	@cd $(RTPTRANS_DIR); rm -f *.o
	@cd $(RELAY_DIR); rm -f *.o
	@cd $(NETWORK_DIR); rm -f *.o
	@cd $(CMD_PROTOCOL_DIR); rm -f *.o
	@cd $(COMMON_DIR); rm -f *.o

	@echo -e "\033[32m[INF] Clean interlive successfully. \033[0m"
	@echo

cleanxmode:
	@find . -regex ".*\.cpp\|.*\.h\|.*\.c" -perm /a+x | xargs chmod -x
	
	@echo -e "\033[32m[INF] Clean x mode successfully. \033[0m"

cleangcov:
	rm -f *.gcda *.gcno
	@cd $(UTIL_DIR);make cleangcov

cleanall:clean-lib clean cleangcov

libcachemanager.a: $(MEDIA_MANAGER_DIR)/cache_manager.h $(MEDIA_MANAGER_DIR)/circular_cache.h $(MEDIA_MANAGER_DIR)/cache_manager_config.h $(MEDIA_MANAGER_OBJS)
	ar rcs libcachemanager.a $(MEDIA_MANAGER_OBJS)

libfragment.a: $(FRAGMENT_DIR)/fragment.h $(FRAGMENT_DIR)/fragment_generator.h $(FRAGMENT_OBJS)
	ar rcs libfragment.a $(FRAGMENT_OBJS)
TARGETTYPE := APP
TARGETNAME := 
