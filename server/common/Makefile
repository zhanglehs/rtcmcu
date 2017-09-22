include env.mk
SOURCES = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SOURCES))
CFLAGS += $(COMMCFLAGS)

all:$(OBJS)

%.o:%.c
	$(CC) -c $(CFLAGS) $(CINCS) $^ -o $@

%.o:%.cpp
	$(CC) -c $(CFLAGS) $(CINCS) $^ -o $@

clean:
	rm *.o -f

cleangcov:
	rm -f *.gcno *.gcda

cleanall:clean cleangcov
