#CC = gcc
CC = g++
CFLAGS = -g -m64 -Wall -fno-strict-aliasing -fomit-frame-pointer -Wfloat-equal -rdynamic -Wformat=2  -Wno-cast-qual -pipe -DHAVE_SCHED_GETAFFINITY
