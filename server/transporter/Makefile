ARCH := $(shell uname -m)
X64 = x86_64

VERSION = 1.2.0
DISTDIR = transporter-${VERSION}
TARBALL = ${DISTDIR}.tar.gz

ifeq ($(ARCH), $(X64))
	LIBS = -lexpat -levent
	REPORTER_LIBS = -levent
	DEFINE = -DVERSION=\"$(VERSION)\" -DHAVE_SCHED_GETAFFINITY
	CFLAGS = -m64 -Wall -O0 -pipe ${DEFINE} -g -I/usr/local/include
else
	LIBS = -lexpat -levent -L/usr/local/lib
	REPORTER_LIBS = -levent -L/usr/local/lib
	DEFINE = -DVERSION=\"$(VERSION)\"
	CFLAGS = -Wall -O0 -pipe ${DEFINE} -g -I/usr/local/include
endif

CC = gcc
TRANSPORTER = transporter
REPORTER = reporter
SUPERVISOR = supervisor
FLVREADER = flvreader
RECEIVER = receiver
DVB = dvb_receiver
TRANSPORTER_OBJS = transporter.o xml.o control.o client.o backend.o buffer.o ipad.o
SUPERVISOR_OBJS = supervisor.o xml.o
REPORTER_OBJS = reporter.o
FLVREADER_OBJS = flvreader.o xml.o buffer.o
RECEIVER_OBJS = receiver.o xml.o buffer.o
DVB_OBJS = dvb.o xml.o buffer.o
ALL = $(TRANSPORTER) $(SUPERVISOR) $(REPORTER) $(FLVREADER) $(RECEIVER) $(DVB)

INSTALL = /usr/bin/install

all: $(ALL)

$(TRANSPORTER): $(TRANSPORTER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(FLVREADER): $(FLVREADER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(SUPERVISOR): $(SUPERVISOR_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(REPORTER): $(REPORTER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(REPORTER_LIBS)

$(RECEIVER): $(RECEIVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(DVB): $(DVB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

transporter.o: transporter.c proto.h transporter.h buffer.h
	$(CC) $(CFLAGS) -c -o $@ $<

buffer.o: buffer.c buffer.h
	$(CC) $(CFLAGS) -c -o $@ $<

xml.o: xml.c xml.h
	$(CC) $(CFLAGS) -c -o $@ $<

control.o: control.c transporter.h buffer.h proto.h
	$(CC) $(CFLAGS) -c -o $@ $<

backend.o: backend.c transporter.h buffer.h
	$(CC) $(CFLAGS) -c -o $@ $<

client.o: client.c transporter.h buffer.h
	$(CC) $(CFLAGS) -c -o $@ $<

ipad.o: ipad.c transporter.h buffer.h
	$(CC) $(CFLAGS) -c -o $@ $<

supervisor.o: supervisor.c proto.h
	$(CC) $(CFLAGS) -c -o $@ $<

dvb.o: dvb.c  buffer.h xml.h
	$(CC) $(CFLAGS) -c -o $@ $<

reporter.o: reporter.c
	$(CC) $(CFLAGS) -c -o $@ $<

flvreader.o: flvreader.c xml.h buffer.h
	$(CC) $(CFLAGS) -c -o $@ $<

receiver.o: receiver.c xml.h buffer.h
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(TRANSPORTER)
	$(INSTALL) -s -m 0755 $(TRANSPORTER) $(DESTDIR)/usr/sbin

clean:
	rm -f *.o $(ALL)

dist: clean
	rm -fr ${DISTDIR}
	mkdir ${DISTDIR}
	cp Makefile *.c *.h ${DISTDIR}
	tar cfz ${DISTDIR}.tar.gz ${DISTDIR}
	rm -fr ${DISTDIR}

