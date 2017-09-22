# Compiler flags
CC = gcc
CPP = g++
CFLAGS = -g -O2 -Wall -Wstrict-prototypes -pipe
CPPFLAGS = -g -O2 -Wall -pipe
AR = ar

all: libstat.a

stat_proc_pid.o: stat_proc_pid.cpp
	$(CPP) -o $@ -c $(CPPFLAGS) $<

status_proc_pid.o: status_proc_pid.cpp
	$(CPP) -o $@ -c $(CPPFLAGS) $<

st_common.o: st_common.c
	$(CC) -o $@ -c $(CFLAGS) $<

st_ioconf.o: st_ioconf.c
	$(CC) -o $@ -c $(CFLAGS) $<

st_rdstats.o: st_rdstats.c
	$(CC) -o $@ -c $(CFLAGS) $<

libstat.a: stat_proc_pid.o status_proc_pid.o st_common.o st_ioconf.o st_rdstats.o
	$(AR) -rvs $@ $^

clean:
	-rm *.o *.a

# Phony targets
.PHONY: all clean
