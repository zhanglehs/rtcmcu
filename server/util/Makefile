include env.mk
SOURCE_DIR:= ../..
include ../../lib/makefile.incl

THIRD_PARTY_DIR = $(SOURCE_DIR)/third_party

CFLAGS += $(OPSCFLAGS)
CINCS += -I$(THIRD_PARTY_DIR)/json-c-0.11 \
         -I$(THIRD_PARTY_DIR)/libevent-2.1.8-stable/out/include \
		 $(INC_UTILS)
SOURCES_C = $(wildcard *.c)
SOURCES_CPP = $(wildcard *.cpp)
OBJS = $(patsubst %.c,%.o,$(SOURCES_C)) $(patsubst %.cpp,%.o,$(SOURCES_CPP))
UNIT_TESTS_DIR = unit_tests

all:objs

objs:$(OBJS)

%.o:%.c
	$(CC) -c $(CFLAGS) $(CINCS) $^ -o $@

%.o:%.cpp
	$(CC) -c $(CFLAGS) $(CINCS) $^ -o $@

unit_tests:$(OBJS)
	@cd $(UNIT_TESTS_DIR); \
	make unit_tests; \
	make run	

clean:
	rm *.o -f

cleangcov:
	rm -f *.gcno *.gcda

clean_unit_tests:
	@cd $(UNIT_TESTS_DIR); \
	make clean

cleanall:clean cleangcov
