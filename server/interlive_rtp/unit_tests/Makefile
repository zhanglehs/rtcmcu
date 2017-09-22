include env.mk
USRLIB = ../../lib
CFLAGS += 
GTEST_DIR = $(USRLIB)/gtest-1.7.0
CINCS += -I $(GTEST_DIR)/include -I ../ -I ../../ -I $(USRLIB)/
GTEST_LIB = $(GTEST_DIR)/lib/.libs/libgtest.a
GTEST_MAIN_LIB = $(GTEST_DIR)/lib/.libs/libgtest_main.a

LIBJSON = $(USRLIB)/json/.libs/libjson-c.a
LIBEVENT = $(USRLIB)/event/.libs/libevent.a

COMMON_DIR = ../../common

UTIL_DIR = ../../util

TEST_SOURCES = $(wildcard *.cpp)

UTIL_LIBS = $(COMMON_DIR)/proto.o $(UTIL_DIR)/log.o $(UTIL_DIR)/buffer.o $(UTIL_DIR)/xml.o \
			 $(UTIL_DIR)/tsmux.o $(UTIL_DIR)/flv2ts.o $(UTIL_DIR)/memory.o $(UTIL_DIR)/session.o \
			 $(UTIL_DIR)/md5.o $(UTIL_DIR)/access.o $(UTIL_DIR)/hashtable.o $(UTIL_DIR)/rbtree.o \
			 $(UTIL_DIR)/util.o $(UTIL_DIR)/backtrace.o $(UTIL_DIR)/levent.o \
			 $(UTIL_DIR)/statfile.o $(UTIL_DIR)/report.o $(UTIL_DIR)/flv.o \
			 $(LIBJSON) $(LIBEVENT) -lrt -lexpat -lgcov

BASECOMMONOBJS = ../connection.o $(UTIL_LIBS)

COMMONOBJS = $(BASECOMMONOBJS) ../hls.o ../stream_manager.o ../media_buffer.o ../http_server.o \
			 ../stream_recorder.o ../cache_manager.o ../fragment.o ../forward_client_v2.o 

PLAYEROBJS = ../module_player.o ../module_fragment_player.o 	
	
FORWARDCOMMONOBJS = ../forward_common.o 

BACKENDOBJS = $(FORWARDCOMMONOBJS) ../module_backend.o ../forward_server_v2.o ../forward_client.o ../forward_server.o

UPLOADEROBJS = ../module_uploader.o

PRODUCT_OBJS = $(BACKENDOBJS) $(PLAYEROBJS) $(COMMONOBJS)

TEST_OBJS = $(patsubst %.cpp,%.o,$(TEST_SOURCES))


all:unit_tests

%.o:%.cpp
	$(CC) -c $(CFLAGS) $(CINCS) $^ -o $@

%.o:%.c
	$(CC) -c $(CFLAGS) $(CINCS) $^ -o $@

$(PRODUCT_OBJS):
	@cd .. ;\
	make test_objs	
	
unit_tests: $(GTEST_LIB) $(GTEST_MAIN_LIB) fragment_test cache_manager_test forward_client_v2_test

fragment_test: fragment_test.o ../fragment.o $(UTIL_LIBS) $(GTEST_LIB) $(GTEST_MAIN_LIB)
	$(CC) $(CFLAGS) fragment_test.o ../fragment.o $(UTIL_LIBS) $(GTEST_LIB) $(GTEST_MAIN_LIB) -lpthread -o fragment_test.test.out

cache_manager_test: cache_manager_test.o ../cache_manager.o ../fragment.o $(UTIL_LIBS) $(GTEST_LIB) $(GTEST_MAIN_LIB)
	$(CC) $(CFLAGS) cache_manager_test.o ../cache_manager.o ../fragment.o $(UTIL_LIBS) $(GTEST_LIB) $(GTEST_MAIN_LIB) -lpthread -o cache_manager_test.test.out

forward_client_v2_test: forward_client_v2_test.o  $(FORWARDCOMMONOBJS) $(COMMONOBJS) $(UTIL_LIBS) $(GTEST_LIB) $(GTEST_MAIN_LIB)
	$(CC) $(CFLAGS) forward_client_v2_test.o ../forward_client_v2.o $(GTEST_LIB) $(GTEST_MAIN_LIB) $(FORWARDCOMMONOBJS) $(BASECOMMONOBJS) -lpthread -o forward_client_v2_test.out
	
$(GTEST_MAIN_LIB): $(GTEST_LIB)	
	
$(GTEST_LIB):
	@cd $(GTEST_DIR); \
	chmod u+x configure; \
	if [ ! -f "config.status" ]; then \
		./configure; \
	else \
		echo "GTEST: config.status file exist, skip configure."; \
	fi; \
	make

clean:
	rm *.o -f
	rm *.test.out -f

cleanall:clean 

