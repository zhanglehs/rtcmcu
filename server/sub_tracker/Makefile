SOURCE_DIR:= ../..
include ../../lib/makefile.incl 

CC = g++
CFLAGS = -g -m64 -Wall -fomit-frame-pointer -pipe -DHAVE_SCHED_GETAFFINITY
CINCS = -I../ $(INC_PROTOBUF) $(INC_EVENT) $(INC_JSONC) $(INC_UTILS) $(INC_REACTOR) $(INC_LOG4CPP)

%.o:%.cpp
	$(CC) -c $(CFLAGS) $(CINCS) $^ -o $@
%.o:%.cc
	$(CC) -c $(CFLAGS) $(CINCS) $^ -o $@

ifndef release
#export COMMCFLAGS = -O0 -fprofile-arcs -ftest-coverage
#export COMMCFLAGS = -O0 -fprofile-arcs
export COMMCFLAGS = -O0
#export COMMLIBS = -lgcov
export COMMLIBS = 
export COMMDEF = -DDEBUG
else
export COMMCFLAGS = -O2
endif


UTIL_DIR = ../util
COMMON_DIR = ../common
PROTOBUF_DIR = ../common/protobuf
USRLIB = ../lib
TRACKER = tracker

CFLAGS += $(COMMCFLAGS)


TRACKERLIBS =  $(STATIC_LIB_PROTOBUF) $(STATIC_LIB_EVENT) $(STATIC_LIB_JSONC) $(STATIC_LIB_REACTOR) $(STATIC_LIB_LOG4CPP)
LINKLIBS = -lexpat -lrt
TRACKEROBJS = main.o tracker.o forward_manager_v3.o tracker_stat.o tracker_config.o http_server.o portal.o stream_manager.o $(PROTOBUF_DIR)/InterliveServerStat.pb.o $(PROTOBUF_DIR)/st2t_fpinfo_packet.pb.o $(PROTOBUF_DIR)/f2t_register_req.pb.o ../util/xml.o ../../lib/utils/buffer.o ../util/log.o ../../lib/utils/memory.o ../util/md5.o ../util/util.o ../util/levent.o ../util/hashtable.o ../util/rbtree.o ../util/statfile.o ../common/proto.o ../util/daemon.o ../util/ketama_hash.o ../../lib/utils/logging.o

$(TRACKER):$(TRACKERLIBS) util_o common_o $(TRACKEROBJS) 
	$(CC) $(CFLAGS) $(CINCS) $(TRACKEROBJS) $(TRACKERLIBS) $(LINKLIBS) $(COMMLIBS) $(COMMDEF) -o $@

$(STATIC_LIB_JSONC):
	make -C $(DIR_LIB) static-json-c

$(STATIC_LIB_EVENT):
	make -C $(DIR_LIB) static-libevent

all: $(TRACKER)

lcov:
	lcov --directory ../ --capture --output-file tracker.info
	genhtml -o tresults tracker.info

util_o:
	@cd $(UTIL_DIR);make all;

common_o:
	@cd $(COMMON_DIR);make all;

clean-lib:
	@cd $(DIR_LIB); make clean-libevent; make clean-json-c; make clean-libstat;
	@echo -e "\033[32m[INF] Clean lib successfully. \033[0m"
	@echo

clean:
	rm -f ${TRACKER} *.o $(UTIL_DIR)/*.o $(COMMON_DIR)/*.o
	@cd $(UTIL_DIR);make clean
	@cd $(COMMON_DIR);make clean
	@cd $(PROTOBUF_DIR); rm -f *.o
#	rm -f *.gcda *.gcno $(UTIL_DIR)/*.gcda $(UTIL_DIR)/*.gcno $(COMMON_DIR)/*.gcda $(COMMON_DIR)/*.gcno

cleangcov:
	rm -f *.gcda *.gcno
	@cd $(UTIL_DIR);make cleangcov
	@cd $(COMMON_DIR);make cleangcov

cleanall:clean cleangcov clean-lib

