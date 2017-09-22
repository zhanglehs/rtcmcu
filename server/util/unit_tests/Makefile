include env.mk
USRLIB = ../../lib
CFLAGS += 
GTEST_DIR = $(USRLIB)/gtest-1.7.0
CINCS += -I $(GTEST_DIR)/include -I ../ -I ../../
GTEST_LIB = $(GTEST_DIR)/lib/.libs/libgtest.a
GTEST_MAIN_LIB = $(GTEST_DIR)/lib/.libs/libgtest_main.a

PRODUCT_SOURCES_C = $(wildcard ../*.c) 
PRODUCT_SOURCES_CPP = $(wildcard ../*.cpp)
TEST_SOURCES = $(wildcard *.cpp)

PRODUCT_OBJS = $(patsubst %.c,%.o,$(PRODUCT_SOURCES_C) $(patsubst %.cpp,%.o,$(PRODUCT_SOURCES_CPP)))
TEST_OBJS = $(patsubst %.cpp,%.o,$(TEST_SOURCES))

all:unit_tests

%.o:%.cpp
	$(CC) -c $(CFLAGS) $(CINCS) $^ -o $@

%.o:%.c
	$(CC) -c $(CFLAGS) $(CINCS) $^ -o $@

$(PRODUCT_OBJS):
	@cd .. ;\
	make objs

unit_tests: $(GTEST_LIB) $(GTEST_MAIN_LIB) linked_hash_map_test fifo_hash_map_test


linked_hash_map_test:  linked_hash_map_test.o $(PRODUCT_OBJS)
	$(CC) $(CFLAGS) linked_hash_map_test.o $(GTEST_LIB) $(GTEST_MAIN_LIB) -lpthread -o linked_hash_map_test.test.out

fifo_hash_map_test:  fifo_hash_map_test.o $(PRODUCT_OBJS)
	$(CC) $(CFLAGS) fifo_hash_map_test.o $(GTEST_LIB) $(GTEST_MAIN_LIB) -lpthread -o fifo_hash_map_test.out



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

