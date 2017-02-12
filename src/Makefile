CC = gcc
CFLAGS = -Wall -O2 -g -I../lib/include/
LDFLAGS = -lpthread -lufp
TARGET = ufpd

SRCS = main.c thread.c epoll.c fib.c forward.c \
hash.c lpm.c neigh.c netlink.c
OBJS = $(subst .c,.o,$(SRCS))

${TARGET}: ${OBJS}
	$(CC) ${LDFLAGS} -o $@ $^

.c.o:
	$(CC) ${CFLAGS} -o $@ -c $<

.PHONY: clean
clean:
	rm -f ${TARGET} ${OBJS}
