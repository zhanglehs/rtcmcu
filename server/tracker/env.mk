#CC = gcc
CC = g++
CFLAGS = -g -m64 -Wall -fomit-frame-pointer -pipe -DHAVE_SCHED_GETAFFINITY
CINCS = -I../ -I../../lib/protobuf/src -I../lib/json -I../../lib

%.o:%.cpp
	$(CC) -c $(CFLAGS) $(CINCS) $^ -o $@

%.o:%.cc
	$(CC) -c $(CFLAGS) $(CINCS) $^ -o $@
